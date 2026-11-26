#include "dishcliclient.h"

#include <QRegularExpression>
#include <QTimer>

#include <cmath>
#include <optional>

namespace {
constexpr int LoginTimeoutMs = 15000;
constexpr int PositionPollIntervalMs = 1000;
constexpr int PositionResponseTimeoutMs = 3000;
constexpr qsizetype MaxResponseBufferSize = 64 * 1024;
constexpr unsigned char TelnetIac = 255;
constexpr unsigned char TelnetDo = 253;
constexpr unsigned char TelnetDont = 254;
constexpr unsigned char TelnetWill = 251;
constexpr unsigned char TelnetWont = 252;
constexpr unsigned char TelnetSubnegotiation = 250;
constexpr unsigned char TelnetSubnegotiationEnd = 240;

const QByteArray AzimuthCommand = "show status antenna earth_az";
const QByteArray ElevationCommand = "show status antenna earth_el";

std::optional<double> parseAngle(QByteArray response, const QByteArray &command)
{
    response.replace('\0', "");
    response.replace('\r', '\n');
    static const QRegularExpression anglePattern(
        QStringLiteral(R"(^(?:(?:status\s+antenna\s+)?(earth_(?:az|el))\s*[:=]\s*)?([+-]?(?:\d+(?:\.\d*)?|\.\d+))(?:\s*(?:deg|°))?$)"),
        QRegularExpression::CaseInsensitiveOption);
    std::optional<double> angle;
    for (QByteArray line : response.split('\n')) {
        line = line.trimmed();
        if (line.startsWith('>'))
            line = line.mid(1).trimmed();
        if (line.isEmpty() || line.compare(command, Qt::CaseInsensitive) == 0)
            continue;

        const auto match = anglePattern.match(QString::fromUtf8(line));
        if (!match.hasMatch() || angle
            || (match.hasCaptured(1) && !command.endsWith(match.captured(1).toLower().toLatin1()))) {
            return std::nullopt;
        }
        bool ok = false;
        const double value = match.captured(2).toDouble(&ok);
        if (!ok || !std::isfinite(value))
            return std::nullopt;
        angle = value;
    }
    return angle;
}
}

DishCliClient::DishCliClient(QObject *parent)
    : QObject(parent)
    , timeoutTimer_(new QTimer(this))
    , positionTimer_(new QTimer(this))
{
    timeoutTimer_->setSingleShot(true);
    timeoutTimer_->setInterval(LoginTimeoutMs);
    connect(timeoutTimer_, &QTimer::timeout, this, [this] {
        fail(state_ == State::Ready
                 ? tr("Timed out waiting for a dish position response; disconnected.")
                 : tr("Timed out while connecting or logging in to the dish."));
    });
    positionTimer_->setInterval(PositionPollIntervalMs);
    connect(positionTimer_, &QTimer::timeout, this, &DishCliClient::pollPosition);

    connect(&socket_, &QTcpSocket::connected, this, [this] {
        setState(State::AwaitingUsername);
        timeoutTimer_->start(LoginTimeoutMs);
        readAvailableData();
    });
    connect(&socket_, &QTcpSocket::readyRead,
            this, &DishCliClient::readAvailableData);
    connect(&socket_, &QTcpSocket::disconnected, this, [this] {
        timeoutTimer_->stop();
        clearCredentials();
        readBuffer_.clear();
        if (state_ != State::Disconnected)
            setState(State::Disconnected);
    });
    connect(&socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError error) {
        if (error != QAbstractSocket::RemoteHostClosedError
            && state_ != State::Disconnected) {
            fail(socket_.errorString());
        }
    });
}

DishCliClient::State DishCliClient::state() const
{
    return state_;
}

QString DishCliClient::stateText() const
{
    switch (state_) {
    case State::Disconnected:
        return tr("Disconnected");
    case State::Connecting:
        return tr("Connecting");
    case State::AwaitingUsername:
    case State::AwaitingPassword:
    case State::AwaitingCommandPrompt:
        return tr("Logging in");
    case State::Ready:
        return tr("Connected");
    }
    return tr("Disconnected");
}

void DishCliClient::connectToDish(const QString &host,
                                  quint16 port,
                                  const QString &username,
                                  const QString &password)
{
    if (state_ != State::Disconnected)
        disconnectFromDish();

    if (host.trimmed().isEmpty()) {
        emit errorOccurred(tr("Enter the dish IP address or host name."));
        return;
    }
    if (username.trimmed().isEmpty()) {
        emit errorOccurred(tr("Set the dish CLI username in Settings."));
        return;
    }

    username_ = username.trimmed().toUtf8();
    password_ = password.toUtf8();
    readBuffer_.clear();
    telnetParseState_ = TelnetParseState::Data;
    setState(State::Connecting);
    timeoutTimer_->start();
    socket_.connectToHost(host.trimmed(), port);
}

void DishCliClient::disconnectFromDish()
{
    timeoutTimer_->stop();
    clearCredentials();
    readBuffer_.clear();

    if (socket_.state() == QAbstractSocket::ConnectingState)
        socket_.abort();
    else if (socket_.state() != QAbstractSocket::UnconnectedState)
        socket_.disconnectFromHost();

    setState(State::Disconnected);
}

void DishCliClient::readAvailableData()
{
    const QByteArray decoded = decodeTelnet(socket_.readAll());
    if (state_ == State::Disconnected
        || (state_ == State::Ready && positionQuery_ == PositionQuery::None)) {
        return;
    }
    readBuffer_.append(decoded);
    if (readBuffer_.size() > MaxResponseBufferSize) {
        fail(tr("Dish response exceeded the buffer limit."));
        return;
    }

    if (state_ == State::Ready)
        processPositionResponse();
    else
        processLoginBuffer();
}

QByteArray DishCliClient::decodeTelnet(const QByteArray &input)
{
    QByteArray decoded;
    decoded.reserve(input.size());

    for (const char rawByte : input) {
        const auto byte = static_cast<unsigned char>(rawByte);
        switch (telnetParseState_) {
        case TelnetParseState::Data:
            if (byte == TelnetIac)
                telnetParseState_ = TelnetParseState::Iac;
            else
                decoded.append(rawByte);
            break;
        case TelnetParseState::Iac:
            if (byte == TelnetIac) {
                decoded.append(static_cast<char>(TelnetIac));
                telnetParseState_ = TelnetParseState::Data;
            } else if (byte == TelnetDo || byte == TelnetDont
                       || byte == TelnetWill || byte == TelnetWont) {
                telnetOptionCommand_ = byte;
                telnetParseState_ = TelnetParseState::Option;
            } else if (byte == TelnetSubnegotiation) {
                telnetParseState_ = TelnetParseState::Subnegotiation;
            } else {
                telnetParseState_ = TelnetParseState::Data;
            }
            break;
        case TelnetParseState::Option:
            if (telnetOptionCommand_ == TelnetDo)
                sendTelnetResponse(TelnetWont, byte);
            else if (telnetOptionCommand_ == TelnetWill)
                sendTelnetResponse(TelnetDont, byte);
            telnetParseState_ = TelnetParseState::Data;
            break;
        case TelnetParseState::Subnegotiation:
            if (byte == TelnetIac)
                telnetParseState_ = TelnetParseState::SubnegotiationIac;
            break;
        case TelnetParseState::SubnegotiationIac:
            telnetParseState_ = byte == TelnetSubnegotiationEnd
                                    ? TelnetParseState::Data
                                    : TelnetParseState::Subnegotiation;
            break;
        }
    }

    return decoded;
}

void DishCliClient::processLoginBuffer()
{
    bool consumedPrompt = true;
    while (consumedPrompt && state_ != State::Disconnected
           && state_ != State::Connecting && state_ != State::Ready) {
        consumedPrompt = false;

        if (state_ == State::AwaitingUsername) {
            const qsizetype prompt = readBuffer_.toLower().indexOf("username:");
            if (prompt >= 0) {
                readBuffer_.remove(0, prompt + qsizetype(9));
                if (!sendLine(username_))
                    return;
                setState(State::AwaitingPassword);
                timeoutTimer_->start(LoginTimeoutMs);
                consumedPrompt = true;
            }
        } else if (state_ == State::AwaitingPassword) {
            const qsizetype prompt = readBuffer_.toLower().indexOf("password:");
            if (prompt >= 0) {
                readBuffer_.remove(0, prompt + qsizetype(9));
                if (!sendLine(password_))
                    return;
                setState(State::AwaitingCommandPrompt);
                timeoutTimer_->start(LoginTimeoutMs);
                consumedPrompt = true;
            }
        } else if (state_ == State::AwaitingCommandPrompt && hasCommandPrompt()) {
            readBuffer_.clear();
            timeoutTimer_->stop();
            clearCredentials();
            setState(State::Ready);
            pollPosition();
        }
    }
}

void DishCliClient::pollPosition()
{
    if (state_ == State::Ready && positionQuery_ == PositionQuery::None)
        sendPositionQuery(PositionQuery::Azimuth);
}

void DishCliClient::sendPositionQuery(PositionQuery query)
{
    readBuffer_.clear();
    positionQuery_ = query;
    if (sendLine(query == PositionQuery::Azimuth ? AzimuthCommand : ElevationCommand))
        timeoutTimer_->start(PositionResponseTimeoutMs);
}

void DishCliClient::processPositionResponse()
{
    if (!hasCommandPrompt())
        return;

    timeoutTimer_->stop();
    const bool isAzimuth = positionQuery_ == PositionQuery::Azimuth;
    const auto angle = parseAngle(readBuffer_.left(readBuffer_.lastIndexOf('>')),
                                  isAzimuth ? AzimuthCommand : ElevationCommand);
    readBuffer_.clear();
    positionQuery_ = PositionQuery::None;

    if (!angle || *angle < (isAzimuth ? -180.0 : -90.0)
        || *angle > (isAzimuth ? 360.0 : 90.0)) {
        emit positionUnavailable();
        if (!positionErrorReported_) {
            positionErrorReported_ = true;
            emit errorOccurred(isAzimuth ? tr("Dish returned an invalid EARTH_AZ response.")
                                         : tr("Dish returned an invalid EARTH_EL response."));
        }
        return;
    }

    if (isAzimuth) {
        pendingAzimuth_ = std::fmod(*angle + 360.0, 360.0);
        sendPositionQuery(PositionQuery::Elevation);
    } else {
        positionErrorReported_ = false;
        emit positionReceived(pendingAzimuth_, *angle);
    }
}

bool DishCliClient::sendLine(const QByteArray &line)
{
    QByteArray message = line;
    message.append('\r');
    if (socket_.write(message) != message.size()) {
        fail(tr("Could not send data to the dish: %1").arg(socket_.errorString()));
        return false;
    }
    return true;
}

void DishCliClient::sendTelnetResponse(unsigned char response, unsigned char option)
{
    const char reply[] = {
        static_cast<char>(TelnetIac),
        static_cast<char>(response),
        static_cast<char>(option)
    };
    if (socket_.write(reply, sizeof(reply)) != sizeof(reply))
        fail(tr("Could not negotiate the Telnet connection: %1").arg(socket_.errorString()));
}

bool DishCliClient::hasCommandPrompt() const
{
    const qsizetype prompt = readBuffer_.lastIndexOf('>');
    if (prompt < 0)
        return false;

    const bool startsLine = prompt == 0
                            || readBuffer_.at(prompt - 1) == '\n'
                            || readBuffer_.at(prompt - 1) == '\r';
    return startsLine && readBuffer_.mid(prompt + 1).trimmed().isEmpty();
}

void DishCliClient::fail(const QString &message)
{
    if (state_ == State::Disconnected)
        return;

    timeoutTimer_->stop();
    clearCredentials();
    readBuffer_.clear();
    socket_.abort();
    setState(State::Disconnected);
    emit errorOccurred(message);
}

void DishCliClient::clearCredentials()
{
    username_.fill('\0');
    username_.clear();
    password_.fill('\0');
    password_.clear();
}

void DishCliClient::setState(State state)
{
    if (state_ == state)
        return;

    const bool wasReady = state_ == State::Ready;
    state_ = state;
    if (state_ == State::Ready) {
        positionErrorReported_ = false;
        positionTimer_->start();
    } else if (wasReady) {
        positionTimer_->stop();
        positionQuery_ = PositionQuery::None;
        emit positionUnavailable();
    }
    emit stateChanged(state_, stateText());
}
