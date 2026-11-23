#ifndef SCANCONTROLLER_H
#define SCANCONTROLLER_H

#include <QObject>
#include <QString>

class ScanController : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Scanning,
        Paused
    };
    Q_ENUM(State)

    explicit ScanController(QObject *parent = nullptr);

    State state() const;
    QString stateName() const;
    bool startScan();
    bool stopScan();

public slots:
    void pauseForIpcLoss();

signals:
    void stateChanged(ScanController::State state);
    void scanStartRequested();
    void scanStopRequested();
    void scanPauseRequested();

private:
    void setState(State state);

    State state_ = State::Stopped;
};

#endif // SCANCONTROLLER_H
