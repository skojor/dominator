#include "mainwindow.h"

#include "dishcliclient.h"
#include "dishdirectionindicator.h"
#include "dishsettings.h"
#include "dishsettingsdialog.h"
#include "elevationindicator.h"
#include "localipcserver.h"
#include "positionreporter.h"
#include "satellitepreviewdialog.h"
#include "scancontroller.h"

#include <QAction>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ipcServer_ = new LocalIpcServer(QStringLiteral("Dominator-Hauken-v1"), this);
    positionReporter_ = new PositionReporter(ipcServer_, this);
    scanController_ = new ScanController(this);
    dishSettings_ = new DishSettings(this);
    dishCliClient_ = new DishCliClient(this);

    setWindowTitle(tr("Dominator - Satellite Dish Control"));
    resize(760, 650);

    auto *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    auto *dishSettingsAction = settingsMenu->addAction(tr("Dish connection…"));
    connect(dishSettingsAction, &QAction::triggered, this, [this] {
        DishSettingsDialog dialog(dishSettings_, this);
        dialog.exec();
    });

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto *previewAction = toolsMenu->addAction(tr("Satellite pointing preview…"));
    auto *previewDialog = new SatellitePreviewDialog(dishSettings_, this);
    connect(previewAction, &QAction::triggered, previewDialog, [previewDialog] {
        previewDialog->show();
        previewDialog->raise();
        previewDialog->activateWindow();
    });

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *connectionGroup = new QGroupBox(tr("Dish connection"), centralWidget);
    auto *connectionLayout = new QHBoxLayout(connectionGroup);
    auto *addressInput = new QLineEdit(connectionGroup);
    addressInput->setPlaceholderText(tr("Dish IP address"));
    addressInput->setText(dishSettings_->dishHost());
    addressInput->setToolTip(
        tr("The dish CLI uses unencrypted Telnet. Connect only on a trusted network."));
    auto *connectButton = new QPushButton(tr("Connect"), connectionGroup);
    auto *disconnectButton = new QPushButton(tr("Disconnect"), connectionGroup);
    disconnectButton->setEnabled(false);
    connectionLayout->addWidget(new QLabel(tr("IP address:"), connectionGroup));
    connectionLayout->addWidget(addressInput, 1);
    connectionLayout->addWidget(connectButton);
    connectionLayout->addWidget(disconnectButton);
    mainLayout->addWidget(connectionGroup);

    auto *scanGroup = new QGroupBox(tr("Scan settings"), centralWidget);
    auto *scanLayout = new QFormLayout(scanGroup);

    const auto addDegreeInput = [scanGroup, scanLayout](const QString &label,
                                                        double value) {
        auto *input = new QDoubleSpinBox(scanGroup);
        input->setRange(-36000.0, 36000.0);
        input->setDecimals(1);
        input->setSuffix(tr("°"));
        input->setValue(value);
        scanLayout->addRow(label, input);
        return input;
    };

    auto *azimuthStartInput =
        addDegreeInput(tr("Azimuth start:"), dishSettings_->azimuthStart());
    auto *azimuthEndInput =
        addDegreeInput(tr("Azimuth end:"), dishSettings_->azimuthEnd());

    auto *speedInput = new QDoubleSpinBox(scanGroup);
    speedInput->setRange(0.1, 360.0);
    speedInput->setDecimals(1);
    speedInput->setSuffix(tr("°/min"));
    speedInput->setValue(dishSettings_->scanSpeed());
    scanLayout->addRow(tr("Scan speed:"), speedInput);

    auto *elevationMinimumInput =
        addDegreeInput(tr("Elevation minimum:"), dishSettings_->elevationMinimum());
    auto *elevationMaximumInput =
        addDegreeInput(tr("Elevation maximum:"), dishSettings_->elevationMaximum());

    auto *scanButtonsLayout = new QHBoxLayout;
    scanButtonsLayout->addStretch();
    auto *startScanButton = new QPushButton(tr("Start scan"), scanGroup);
    auto *stopScanButton = new QPushButton(tr("Stop scan"), scanGroup);
    scanButtonsLayout->addWidget(startScanButton);
    scanButtonsLayout->addWidget(stopScanButton);
    scanLayout->addRow(scanButtonsLayout);
    mainLayout->addWidget(scanGroup);

    auto *statusGroup = new QGroupBox(tr("Dish status"), centralWidget);
    auto *statusLayout = new QHBoxLayout(statusGroup);
    auto *directionGroup = new QGroupBox(tr("Azimuth — true north"), statusGroup);
    auto *directionLayout = new QVBoxLayout(directionGroup);
    directionIndicator_ = new DishDirectionIndicator(directionGroup);
    directionLayout->addWidget(directionIndicator_);
    statusLayout->addWidget(directionGroup, 1);

    auto *elevationGroup = new QGroupBox(tr("Elevation — side view"), statusGroup);
    auto *elevationLayout = new QVBoxLayout(elevationGroup);
    elevationIndicator_ = new ElevationIndicator(elevationGroup);
    elevationLayout->addWidget(elevationIndicator_);
    statusLayout->addWidget(elevationGroup, 1);

    auto *readoutsGroup = new QGroupBox(tr("Readouts"), statusGroup);
    auto *readoutsLayout = new QFormLayout(readoutsGroup);
    scanStateLabel_ = new QLabel(tr("Stopped"), readoutsGroup);
    ipcStatusLabel_ = new QLabel(readoutsGroup);
    dishCliStatusLabel_ = new QLabel(dishCliClient_->stateText(), readoutsGroup);
    azimuthLabel_ = new QLabel(tr("— °"), readoutsGroup);
    elevationLabel_ = new QLabel(tr("— °"), readoutsGroup);
    signalLevelLabel_ = new QLabel(tr("—"), readoutsGroup);
    readoutsLayout->addRow(tr("Scan state:"), scanStateLabel_);
    readoutsLayout->addRow(tr("Dish CLI:"), dishCliStatusLabel_);
    readoutsLayout->addRow(tr("Hauken IPC:"), ipcStatusLabel_);
    readoutsLayout->addRow(tr("Current azimuth:"), azimuthLabel_);
    readoutsLayout->addRow(tr("Current elevation:"), elevationLabel_);
    readoutsLayout->addRow(tr("Signal level:"), signalLevelLabel_);
    statusLayout->addWidget(readoutsGroup);
    mainLayout->addWidget(statusGroup);
    mainLayout->addStretch();

    setCentralWidget(centralWidget);
    statusBar()->showMessage(tr("Not connected"));

    const auto saveFrontPageSettings = [this, addressInput, azimuthStartInput,
                                        azimuthEndInput, speedInput,
                                        elevationMinimumInput, elevationMaximumInput] {
        QString error;
        if (!dishSettings_->saveFrontPageSettings(
                addressInput->text(), azimuthStartInput->value(),
                azimuthEndInput->value(), speedInput->value(),
                elevationMinimumInput->value(), elevationMaximumInput->value(),
                error)) {
            statusBar()->showMessage(error, 10000);
        }
    };
    connect(addressInput, &QLineEdit::textChanged,
            this, saveFrontPageSettings);
    connect(azimuthStartInput, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, saveFrontPageSettings);
    connect(azimuthEndInput, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, saveFrontPageSettings);
    connect(speedInput, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, saveFrontPageSettings);
    connect(elevationMinimumInput, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, saveFrontPageSettings);
    connect(elevationMaximumInput, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, saveFrontPageSettings);

    connect(startScanButton, &QPushButton::clicked,
            scanController_, &ScanController::startScan);
    connect(stopScanButton, &QPushButton::clicked,
            scanController_, &ScanController::stopScan);
    connect(connectButton, &QPushButton::clicked, this, [this, addressInput] {
        QString password;
        QString error;
        if (!dishSettings_->loadPassword(password, error)) {
            QMessageBox::warning(this, tr("Dish password unavailable"), error);
            return;
        }

        dishCliClient_->connectToDish(addressInput->text(),
                                      static_cast<quint16>(dishSettings_->tcpPort()),
                                      dishSettings_->username(), password);
        password.fill(QChar(0));
    });
    connect(disconnectButton, &QPushButton::clicked,
            dishCliClient_, &DishCliClient::disconnectFromDish);
    connect(dishCliClient_, &DishCliClient::stateChanged, this,
            [this, connectButton, disconnectButton](DishCliClient::State state,
                                                     const QString &description) {
        dishCliStatusLabel_->setText(description);
        const bool disconnected = state == DishCliClient::State::Disconnected;
        connectButton->setEnabled(disconnected);
        disconnectButton->setEnabled(!disconnected);
    });
    connect(dishCliClient_, &DishCliClient::errorOccurred, this,
            [this](const QString &error) {
        statusBar()->showMessage(error, 12000);
    });
    connect(ipcServer_, &LocalIpcServer::statusChanged,
            ipcStatusLabel_, &QLabel::setText);
    ipcStatusLabel_->setText(ipcServer_->statusText());

    connect(ipcServer_, &LocalIpcServer::clientDisconnected,
            scanController_, &ScanController::pauseForIpcLoss);
    connect(ipcServer_, &LocalIpcServer::clientConnected, this, [this] {
        ipcServer_->sendScanState(scanController_->stateName());
    });
    connect(ipcServer_, &LocalIpcServer::signalLevelReceived, this,
            [this](double levelDbUv, const QString &) {
        signalLevelLabel_->setText(QString::number(levelDbUv, 'f', 1) + tr(" dBuV"));
    });
    connect(ipcServer_, &LocalIpcServer::startScanRequested, this,
            [this](const QString &requestId) {
        scanController_->startScan();
        ipcServer_->sendCommandResult(requestId, QStringLiteral("start"), true,
                                      scanController_->stateName());
    });
    connect(ipcServer_, &LocalIpcServer::stopScanRequested, this,
            [this](const QString &requestId) {
        scanController_->stopScan();
        ipcServer_->sendCommandResult(requestId, QStringLiteral("stop"), true,
                                      scanController_->stateName());
    });
    connect(scanController_, &ScanController::stateChanged, this,
            [this](ScanController::State state) {
        Q_UNUSED(state);
        scanStateLabel_->setText(scanController_->stateName());
        ipcServer_->sendScanState(scanController_->stateName());
    });
    connect(dishCliClient_, &DishCliClient::positionReceived,
            this, &MainWindow::updateDishPosition);
    connect(dishCliClient_, &DishCliClient::positionUnavailable,
            positionReporter_, &PositionReporter::clearPosition);
    connect(positionReporter_, &PositionReporter::positionUpdated,
            previewDialog, &SatellitePreviewDialog::setActualPosition);
    connect(positionReporter_, &PositionReporter::positionCleared,
            previewDialog, &SatellitePreviewDialog::clearActualPosition);
    connect(positionReporter_, &PositionReporter::positionCleared, this, [this] {
        azimuthLabel_->setText(tr("— °"));
        elevationLabel_->setText(tr("— °"));
        directionIndicator_->clearAzimuth();
        elevationIndicator_->clearElevation();
    });
    connect(positionReporter_, &PositionReporter::positionUpdated, this,
            [this](double azimuthDegrees, double elevationDegrees) {
        azimuthLabel_->setText(QString::number(azimuthDegrees, 'f', 1) + tr(" °"));
        elevationLabel_->setText(QString::number(elevationDegrees, 'f', 1) + tr(" °"));
        directionIndicator_->setAzimuthDegrees(azimuthDegrees);
        elevationIndicator_->setElevationDegrees(elevationDegrees);
    });
    connect(positionReporter_, &PositionReporter::positionRejected,
            this, [this](const QString &reason) {
        statusBar()->showMessage(reason, 10000);
    });
    connect(ipcServer_, &LocalIpcServer::communicationError,
            this, [this](const QString &error) {
        statusBar()->showMessage(tr("IPC error: %1").arg(error), 10000);
    });
    connect(ipcServer_, &LocalIpcServer::protocolError,
            this, [this](const QString &error) {
        statusBar()->showMessage(tr("IPC protocol error: %1").arg(error), 10000);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::updateDishPosition(double azimuthDegrees, double elevationDegrees)
{
    positionReporter_->updatePosition(azimuthDegrees, elevationDegrees);
}
