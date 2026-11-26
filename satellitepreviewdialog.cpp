#include "satellitepreviewdialog.h"

#include "dishsettings.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {
constexpr int RefreshIntervalMs = 120000;
}

SatellitePreviewDialog::SatellitePreviewDialog(DishSettings *settings, QWidget *parent)
    : QDialog(parent)
    , settings_(settings)
    , inputs_(new QWidget(this))
    , catalogId_(new QLineEdit(inputs_))
    , apiKey_(new QLineEdit(inputs_))
    , start_(new QPushButton(tr("Start preview"), this))
    , stop_(new QPushButton(tr("Stop preview"), this))
    , status_(new QLabel(this))
    , pointing_(new QLabel(this))
    , actualPointing_(new QLabel(this))
{
    setWindowTitle(tr("Satellite pointing preview (N2YO)"));
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(tr("Enter the dish location. Starting the preview sends these "
                                "coordinates to N2YO over HTTPS. This does not move the dish."), this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addWidget(inputs_);

    auto *form = new QFormLayout(inputs_);
    catalogId_->setPlaceholderText(tr("e.g. 25544 for the ISS"));
    apiKey_->setEchoMode(QLineEdit::Password);
#ifdef Q_OS_WIN
    apiKey_->setPlaceholderText(tr("Saved encrypted for this Windows user"));
#else
    apiKey_->setPlaceholderText(tr("Not saved to disk"));
#endif
    form->addRow(tr("NORAD catalog ID:"), catalogId_);
    form->addRow(tr("N2YO API key:"), apiKey_);
    const auto coordinate = [&](const QString &label, double min, double max, int decimals) {
        auto *input = new QDoubleSpinBox(inputs_);
        input->setDecimals(decimals);
        input->setRange(min, max);
        form->addRow(label, input);
        return input;
    };
    latitude_ = coordinate(tr("Latitude (° north):"), -90, 90, 6);
    longitude_ = coordinate(tr("Longitude (° east):"), -180, 180, 6);
    altitude_ = coordinate(tr("Altitude above sea level (m):"), -500, 100000, 1);

    pointing_->setTextFormat(Qt::PlainText);
    pointing_->setWordWrap(true);
    status_->setWordWrap(true);
    auto *pointingLayout = new QHBoxLayout;
    pointingLayout->addWidget(pointing_, 1, Qt::AlignTop);
    pointingLayout->addWidget(actualPointing_, 1, Qt::AlignTop);
    layout->addLayout(pointingLayout);
    actualPointing_->setWordWrap(true);
    clearActualPosition();
    layout->addWidget(status_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(start_, QDialogButtonBox::ActionRole);
    buttons->addButton(stop_, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    refreshTimer_.setSingleShot(true);
    refreshTimer_.setInterval(RefreshIntervalMs);
    displayTimer_.setInterval(250);
    connect(&refreshTimer_, &QTimer::timeout, this, &SatellitePreviewDialog::fetch);
    connect(&displayTimer_, &QTimer::timeout, this, &SatellitePreviewDialog::showPointing);
    connect(start_, &QPushButton::clicked, this, &SatellitePreviewDialog::startPreview);
    connect(stop_, &QPushButton::clicked, this, &SatellitePreviewDialog::stopPreview);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::finished, this, [this] {
        stopPreview();
        apiKey_->clear();
    });
    connect(&client_, &N2yoClient::predictionReceived, this, [this](const SatellitePrediction &batch) {
        if (batch.points.last().timestamp >= QDateTime::currentSecsSinceEpoch()) {
            prediction_ = batch;
            status_->setText(tr("Predictions received from N2YO."));
        } else {
            status_->setText(tr("N2YO returned expired predictions. Check the computer clock."));
        }
        showPointing();
        refreshTimer_.start();
    });
    connect(&client_, &N2yoClient::requestFailed, this, [this](const QString &error) {
        status_->setText(tr("%1 Retrying in two minutes.").arg(error));
        refreshTimer_.start();
    });
    stopPreview();
}

void SatellitePreviewDialog::setActualPosition(double azimuthDegrees, double elevationDegrees)
{
    actualPointing_->setText(tr("Actual dish (controller-reported)\nAzimuth: %1°\nElevation: %2°")
                                .arg(azimuthDegrees, 0, 'f', 2)
                                .arg(elevationDegrees, 0, 'f', 2));
}

void SatellitePreviewDialog::clearActualPosition()
{
    actualPointing_->setText(tr("Actual dish\nNo current position."));
}

void SatellitePreviewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (event->spontaneous())
        return;

    const auto preview = settings_->satellitePreview();
    catalogId_->setText(preview.catalogId);
    latitude_->setValue(preview.latitude);
    longitude_->setValue(preview.longitude);
    altitude_->setValue(preview.altitude);

    QString apiKey;
    QString error;
    if (!settings_->loadN2yoApiKey(apiKey, error))
        status_->setText(tr("Could not load the saved N2YO API key. %1 Enter it again.").arg(error));
    apiKey_->setText(apiKey);
}

void SatellitePreviewDialog::startPreview()
{
    bool valid = false;
    const qint64 id = catalogId_->text().trimmed().toLongLong(&valid);
    if (!valid || id < 1 || id > 999999999 || apiKey_->text().trimmed().isEmpty()) {
        status_->setText(tr("Enter a valid NORAD catalog ID and an N2YO API key."));
        return;
    }

    const SatellitePreviewSettings preview{QString::number(id), latitude_->value(),
                                           longitude_->value(), altitude_->value()};
#ifdef Q_OS_WIN
    const QString keyToSave = apiKey_->text().trimmed();
#else
    const QString keyToSave;
#endif
    QString error;
    if (!settings_->saveSatellitePreview(preview, keyToSave, error)) {
        status_->setText(error);
        return;
    }

    inputs_->setEnabled(false);
    start_->setEnabled(false);
    stop_->setEnabled(true);
    displayTimer_.start();
    fetch();
}

void SatellitePreviewDialog::stopPreview()
{
    refreshTimer_.stop();
    displayTimer_.stop();
    client_.cancel();
    prediction_ = {};
    inputs_->setEnabled(true);
    start_->setEnabled(true);
    stop_->setEnabled(false);
    status_->setText(tr("Preview stopped."));
    pointing_->setText(tr("No prediction."));
}

void SatellitePreviewDialog::fetch()
{
    status_->setText(tr("Fetching predictions from N2YO…"));
    client_.fetch(catalogId_->text().trimmed().toLongLong(), latitude_->value(),
                  longitude_->value(), altitude_->value(), apiKey_->text().trimmed());
}

void SatellitePreviewDialog::showPointing()
{
    const auto *point = prediction_.pointAt(QDateTime::currentSecsSinceEpoch());
    if (!point) {
        pointing_->setText(tr("No current prediction."));
        return;
    }

    const auto timestamp = QDateTime::fromSecsSinceEpoch(point->timestamp, Qt::UTC);
    QString label = tr("%1 — %2 UTC\nPredicted azimuth: %3° (true north)\nPredicted elevation: %4°")
        .arg(prediction_.name.isEmpty() ? catalogId_->text().trimmed() : prediction_.name,
             timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .arg(point->azimuth, 0, 'f', 2)
        .arg(point->elevation, 0, 'f', 2);
    if (point->elevation < 0)
        label += tr(" (below horizon)");
    pointing_->setText(label);
}
