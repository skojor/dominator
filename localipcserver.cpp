#include "localipcserver.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>

#include <cmath>

namespace {
constexpr qsizetype MaxMessageSize = 64 * 1024;

QString utcTimestamp(const QDateTime &timestamp)
{
    return timestamp.toUTC().toString(Qt::ISODateWithMs);
}
}

LocalIpcServer::LocalIpcServer(const QString &serverName, QObject *parent)
    : QObject(parent)
    , serverName_(serverName)
    , server_(new QLocalServer(this))
{
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    connect(server_, &QLocalServer::newConnection,
            this, &LocalIpcServer::acceptConnections);

    if (!server_->listen(serverName_)) {
        setStatusText(tr("IPC unavailable: %1").arg(server_->errorString()));
        emit communicationError(statusText_);
        return;
    }

    setStatusText(tr("Waiting for Hauken"));
}

LocalIpcServer::~LocalIpcServer()
{
    if (client_)
        client_->disconnectFromServer();
    if (server_)
        server_->close();
}

bool LocalIpcServer::isListening() const
{
    return server_->isListening();
}

bool LocalIpcServer::hasClient() const
{
    return client_ && client_->state() == QLocalSocket::ConnectedState;
}

QString LocalIpcServer::statusText() const
{
    return statusText_;
}

bool LocalIpcServer::sendPosition(double azimuthDegrees,
                                  double elevationDegrees,
                                  const QDateTime &timestamp)
{
    if (!std::isfinite(azimuthDegrees) || !std::isfinite(elevationDegrees)
        || !timestamp.isValid()) {
        emit protocolError(tr("Cannot send an invalid dish position."));
        return false;
    }

    return writeMessage({
        {QStringLiteral("type"), QStringLiteral("position")},
        {QStringLiteral("azimuth_deg"), azimuthDegrees},
        {QStringLiteral("elevation_deg"), elevationDegrees},
        {QStringLiteral("reference"), QStringLiteral("true_north")},
        {QStringLiteral("timestamp"), utcTimestamp(timestamp)}
    });
}

bool LocalIpcServer::sendScanState(const QString &state)
{
    return writeMessage({
        {QStringLiteral("type"), QStringLiteral("scan_state")},
        {QStringLiteral("state"), state},
        {QStringLiteral("timestamp"), utcTimestamp(QDateTime::currentDateTimeUtc())}
    });
}

bool LocalIpcServer::sendCommandResult(const QString &requestId,
                                       const QString &command,
                                       bool accepted,
                                       const QString &state,
                                       const QString &error)
{
    QJsonObject response{
        {QStringLiteral("type"), QStringLiteral("command_result")},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("command"), command},
        {QStringLiteral("accepted"), accepted},
        {QStringLiteral("state"), state}
    };
    if (!error.isEmpty())
        response.insert(QStringLiteral("error"), error);
    return writeMessage(response);
}

void LocalIpcServer::acceptConnections()
{
    while (server_->hasPendingConnections()) {
        QLocalSocket *socket = server_->nextPendingConnection();
        if (hasClient()) {
            socket->disconnectFromServer();
            socket->deleteLater();
            continue;
        }

        client_ = socket;
        readBuffer_.clear();
        connect(socket, &QLocalSocket::readyRead, this,
                [this, socket] { readMessages(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (client_ != socket) {
                socket->deleteLater();
                return;
            }

            client_ = nullptr;
            readBuffer_.clear();
            setStatusText(tr("Waiting for Hauken"));
            emit clientDisconnected();
            socket->deleteLater();
        });
        connect(socket, &QLocalSocket::errorOccurred, this,
                [this, socket](QLocalSocket::LocalSocketError error) {
            if (client_ == socket && error != QLocalSocket::PeerClosedError)
                emit communicationError(socket->errorString());
        });

        setStatusText(tr("Hauken connected"));
        emit clientConnected();
    }
}

void LocalIpcServer::readMessages(QLocalSocket *socket)
{
    if (socket != client_)
        return;

    readBuffer_.append(socket->readAll());
    while (true) {
        const qsizetype newline = readBuffer_.indexOf('\n');
        if (newline < 0)
            break;
        if (newline > MaxMessageSize) {
            emit protocolError(tr("IPC message exceeds the maximum size."));
            socket->disconnectFromServer();
            return;
        }

        QByteArray line = readBuffer_.left(newline);
        readBuffer_.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (!line.trimmed().isEmpty())
            processMessage(line);
    }

    if (readBuffer_.size() > MaxMessageSize) {
        emit protocolError(tr("IPC message exceeds the maximum size."));
        socket->disconnectFromServer();
    }
}

void LocalIpcServer::processMessage(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        sendProtocolError(QStringLiteral("invalid_json"), parseError.errorString());
        return;
    }

    const QJsonObject message = document.object();
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("signal_level")) {
        const QJsonValue value = message.value(QStringLiteral("level_dBuV"));
        if (!value.isDouble() || !std::isfinite(value.toDouble())) {
            sendProtocolError(QStringLiteral("invalid_signal_level"),
                              tr("signal_level requires a finite level_dBuV number."));
            return;
        }

        QString timestamp = message.value(QStringLiteral("timestamp")).toString();
        if (timestamp.isEmpty()) {
            timestamp = utcTimestamp(QDateTime::currentDateTimeUtc());
        } else {
            QDateTime parsed = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
            if (!parsed.isValid())
                parsed = QDateTime::fromString(timestamp, Qt::ISODate);
            if (!parsed.isValid()) {
                sendProtocolError(QStringLiteral("invalid_timestamp"),
                                  tr("timestamp must be an ISO 8601 date and time."));
                return;
            }
            timestamp = utcTimestamp(parsed);
        }

        emit signalLevelReceived(value.toDouble(), timestamp);
        return;
    }

    if (type == QStringLiteral("scan_command")) {
        const QString requestId = message.value(QStringLiteral("request_id")).toString();
        const QString command = message.value(QStringLiteral("command")).toString();
        if (requestId.isEmpty()) {
            sendProtocolError(QStringLiteral("missing_request_id"),
                              tr("scan_command requires a request_id."));
        } else if (command == QStringLiteral("start")) {
            emit startScanRequested(requestId);
        } else if (command == QStringLiteral("stop")) {
            emit stopScanRequested(requestId);
        } else {
            sendCommandResult(requestId, command, false, QStringLiteral("unknown"),
                              tr("Unsupported scan command."));
        }
        return;
    }

    sendProtocolError(QStringLiteral("unknown_message_type"),
                      tr("Unsupported IPC message type."));
}

bool LocalIpcServer::writeMessage(const QJsonObject &message)
{
    if (!hasClient())
        return false;

    QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact);
    line.append('\n');
    if (client_->write(line) != line.size()) {
        emit communicationError(client_->errorString());
        return false;
    }
    return true;
}

void LocalIpcServer::sendProtocolError(const QString &code, const QString &message)
{
    emit protocolError(message);
    writeMessage({
        {QStringLiteral("type"), QStringLiteral("error")},
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message}
    });
}

void LocalIpcServer::setStatusText(const QString &status)
{
    if (statusText_ == status)
        return;
    statusText_ = status;
    emit statusChanged(statusText_);
}
