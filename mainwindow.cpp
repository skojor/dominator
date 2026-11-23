#include "mainwindow.h"

#include "localipcserver.h"
#include "positionreporter.h"
#include "scancontroller.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

    setWindowTitle(tr("Dominator - Satellite Dish Control"));
    resize(620, 480);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *connectionGroup = new QGroupBox(tr("Dish connection"), centralWidget);
    auto *connectionLayout = new QHBoxLayout(connectionGroup);
    auto *addressInput = new QLineEdit(connectionGroup);
    addressInput->setPlaceholderText(tr("Dish IP address"));
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
    };

    addDegreeInput(tr("Azimuth start:"), 0.0);
    addDegreeInput(tr("Azimuth end:"), 360.0);

    auto *speedInput = new QDoubleSpinBox(scanGroup);
    speedInput->setRange(0.1, 360.0);
    speedInput->setDecimals(1);
    speedInput->setSuffix(tr("°/min"));
    speedInput->setValue(1.0);
    scanLayout->addRow(tr("Scan speed:"), speedInput);

    addDegreeInput(tr("Elevation minimum:"), 0.0);
    addDegreeInput(tr("Elevation maximum:"), 90.0);

    auto *scanButtonsLayout = new QHBoxLayout;
    scanButtonsLayout->addStretch();
    auto *startScanButton = new QPushButton(tr("Start scan"), scanGroup);
    auto *stopScanButton = new QPushButton(tr("Stop scan"), scanGroup);
    scanButtonsLayout->addWidget(startScanButton);
    scanButtonsLayout->addWidget(stopScanButton);
    scanLayout->addRow(scanButtonsLayout);
    mainLayout->addWidget(scanGroup);

    auto *statusGroup = new QGroupBox(tr("Dish status"), centralWidget);
    auto *statusLayout = new QFormLayout(statusGroup);
    scanStateLabel_ = new QLabel(tr("Stopped"), statusGroup);
    ipcStatusLabel_ = new QLabel(statusGroup);
    azimuthLabel_ = new QLabel(tr("— °"), statusGroup);
    elevationLabel_ = new QLabel(tr("— °"), statusGroup);
    signalLevelLabel_ = new QLabel(tr("—"), statusGroup);
    statusLayout->addRow(tr("Scan state:"), scanStateLabel_);
    statusLayout->addRow(tr("Hauken IPC:"), ipcStatusLabel_);
    statusLayout->addRow(tr("Current azimuth:"), azimuthLabel_);
    statusLayout->addRow(tr("Current elevation:"), elevationLabel_);
    statusLayout->addRow(tr("Signal level:"), signalLevelLabel_);
    mainLayout->addWidget(statusGroup);
    mainLayout->addStretch();

    setCentralWidget(centralWidget);
    statusBar()->showMessage(tr("Not connected"));

    connect(startScanButton, &QPushButton::clicked,
            scanController_, &ScanController::startScan);
    connect(stopScanButton, &QPushButton::clicked,
            scanController_, &ScanController::stopScan);
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
    connect(positionReporter_, &PositionReporter::positionUpdated, this,
            [this](double azimuthDegrees, double elevationDegrees) {
        azimuthLabel_->setText(QString::number(azimuthDegrees, 'f', 1) + tr(" °"));
        elevationLabel_->setText(QString::number(elevationDegrees, 'f', 1) + tr(" °"));
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
