#include "scancontroller.h"

ScanController::ScanController(QObject *parent)
    : QObject(parent)
{
}

ScanController::State ScanController::state() const
{
    return state_;
}

QString ScanController::stateName() const
{
    switch (state_) {
    case State::Stopped:
        return QStringLiteral("stopped");
    case State::Scanning:
        return QStringLiteral("scanning");
    case State::Paused:
        return QStringLiteral("paused");
    }
    return QStringLiteral("stopped");
}

bool ScanController::startScan()
{
    if (state_ == State::Scanning)
        return true;

    setState(State::Scanning);
    emit scanStartRequested();
    return true;
}

bool ScanController::stopScan()
{
    if (state_ == State::Stopped)
        return true;

    setState(State::Stopped);
    emit scanStopRequested();
    return true;
}

void ScanController::pauseForIpcLoss()
{
    if (state_ != State::Scanning)
        return;

    setState(State::Paused);
    emit scanPauseRequested();
}

void ScanController::setState(State state)
{
    if (state_ == state)
        return;
    state_ = state;
    emit stateChanged(state_);
}
