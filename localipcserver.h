#ifndef LOCALIPCSERVER_H
#define LOCALIPCSERVER_H

#include <QDateTime>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;
class QJsonObject;

class LocalIpcServer : public QObject
{
    Q_OBJECT

public:
    explicit LocalIpcServer(
        const QString &serverName = QStringLiteral("Dominator-Hauken-v1"),
        QObject *parent = nullptr);
    ~LocalIpcServer() override;

    bool isListening() const;
    bool hasClient() const;
    QString statusText() const;

    bool sendPosition(double azimuthDegrees,
                      double elevationDegrees,
                      const QDateTime &timestamp);
    bool sendScanState(const QString &state);
    bool sendCommandResult(const QString &requestId,
                           const QString &command,
                           bool accepted,
                           const QString &state,
                           const QString &error = QString());

signals:
    void clientConnected();
    void clientDisconnected();
    void statusChanged(const QString &status);
    void communicationError(const QString &error);
    void protocolError(const QString &error);
    void signalLevelReceived(double levelDbUv, const QString &timestamp);
    void startScanRequested(const QString &requestId);
    void stopScanRequested(const QString &requestId);

private:
    void acceptConnections();
    void readMessages(QLocalSocket *socket);
    void processMessage(const QByteArray &line);
    bool writeMessage(const QJsonObject &message);
    void sendProtocolError(const QString &code, const QString &message);
    void setStatusText(const QString &status);

    QString serverName_;
    QString statusText_;
    QLocalServer *server_ = nullptr;
    QLocalSocket *client_ = nullptr;
    QByteArray readBuffer_;
};

#endif // LOCALIPCSERVER_H
