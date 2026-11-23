#include "positionreporter.h"

#include "localipcserver.h"

#include <QDateTime>

#include <cmath>

PositionReporter::PositionReporter(LocalIpcServer *ipcServer, QObject *parent)
    : QObject(parent)
    , ipcServer_(ipcServer)
{
    reportTimer_.setInterval(1000);
    connect(&reportTimer_, &QTimer::timeout,
            this, &PositionReporter::reportPosition);
    connect(ipcServer_, &LocalIpcServer::clientConnected,
            this, &PositionReporter::reportPosition);
    reportTimer_.start();
}

void PositionReporter::updatePosition(double azimuthDegrees, double elevationDegrees)
{
    if (!std::isfinite(azimuthDegrees) || !std::isfinite(elevationDegrees)) {
        emit positionRejected(tr("Dish position must contain finite values."));
        return;
    }

    azimuthDegrees_ = azimuthDegrees;
    elevationDegrees_ = elevationDegrees;
    positionTimestamp_ = QDateTime::currentDateTimeUtc();
    hasPosition_ = true;
    emit positionUpdated(azimuthDegrees_, elevationDegrees_);
}

void PositionReporter::reportPosition()
{
    if (!hasPosition_)
        return;

    ipcServer_->sendPosition(azimuthDegrees_, elevationDegrees_, positionTimestamp_);
}
