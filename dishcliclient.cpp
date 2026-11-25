#include "dishcliclient.h"

#include <QTimer>

namespace {
constexpr int LoginTimeoutMs = 15000;
constexpr qsizetype MaxLoginBufferSize = 64 * 1024;
constexpr unsigned char TelnetIac = 255;
constexpr unsigned char TelnetDo = 253;
constexpr unsigned char TelnetDont = 254;
constexpr unsigned char TelnetWill = 251;
constexpr unsigned char TelnetWont = 252;
constexpr unsigned char TelnetSubnegotiation = 250;
constexpr unsigned char TelnetSubnegotiationEnd = 240;
}

DishCliClient::DishCliClient(QObject *parent)
    : QObject(parent)
    , timeoutTimer_(new QTimer(this))
{
    timeoutTimer_->setSingleShot(true);
    timeoutTimer_->setInterval(LoginTimeoutMs);
    connect(timeoutTimer_, &QTimer::timeout, this, [this] {
        fail(tr("Timed out while connecting or logging in to the dish."));
    });

    connect(&socket_, &QTcpSocket::connected, this, [this] {
        setState(State::AwaitingUsername);
        timeoutTimer_->start();
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
    readBuffer_.append(decoded);
    if (readBuffer_.size() > MaxLoginBufferSize) {
        fail(tr("Dish sent too much data before completing login."));
        return;
    }

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
                timeoutTimer_->start();
                consumedPrompt = true;
            }
        } else if (state_ == State::AwaitingPassword) {
            const qsizetype prompt = readBuffer_.toLower().indexOf("password:");
            if (prompt >= 0) {
                readBuffer_.remove(0, prompt + qsizetype(9));
                if (!sendLine(password_))
                    return;
                setState(State::AwaitingCommandPrompt);
                timeoutTimer_->start();
                consumedPrompt = true;
            }
        } else if (state_ == State::AwaitingCommandPrompt && hasCommandPrompt()) {
            readBuffer_.clear();
            timeoutTimer_->stop();
            clearCredentials();
            setState(State::Ready);
        }
    }
}

bool DishCliClient::sendLine(const QByteArray &line)
{
    QByteArray message = line;
    message.append('\r');
    if (socket_.write(message) != message.size()) {
        fail(tr("Could not send login data to the dish: %1").arg(socket_.errorString()));
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

    state_ = state;
    emit stateChanged(state_, stateText());
}
