#ifndef POSITIONREPORTER_H
#define POSITIONREPORTER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

class LocalIpcServer;

class PositionReporter : public QObject
{
    Q_OBJECT

public:
    explicit PositionReporter(LocalIpcServer *ipcServer, QObject *parent = nullptr);

public slots:
    void updatePosition(double azimuthDegrees, double elevationDegrees);
    void clearPosition();

signals:
    void positionUpdated(double azimuthDegrees, double elevationDegrees);
    void positionCleared();
    void positionRejected(const QString &reason);

private slots:
    void reportPosition();

private:
    LocalIpcServer *ipcServer_;
    QTimer reportTimer_;
    bool hasPosition_ = false;
    double azimuthDegrees_ = 0.0;
    double elevationDegrees_ = 0.0;
    QDateTime positionTimestamp_;
};

#endif // POSITIONREPORTER_H
