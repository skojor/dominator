#ifndef DISHCLICLIENT_H
#define DISHCLICLIENT_H

#include <QObject>
#include <QTcpSocket>

class QTimer;

class DishCliClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        AwaitingUsername,
        AwaitingPassword,
        AwaitingCommandPrompt,
        Ready
    };
    Q_ENUM(State)

    explicit DishCliClient(QObject *parent = nullptr);

    State state() const;
    QString stateText() const;

    void connectToDish(const QString &host,
                       quint16 port,
                       const QString &username,
                       const QString &password);
    void disconnectFromDish();

signals:
    void stateChanged(DishCliClient::State state, const QString &description);
    void errorOccurred(const QString &error);
    void positionReceived(double azimuthDegrees, double elevationDegrees);
    void positionUnavailable();

private:
    enum class TelnetParseState {
        Data,
        Iac,
        Option,
        Subnegotiation,
        SubnegotiationIac
    };

    enum class PositionQuery { None, Azimuth, Elevation };

    void readAvailableData();
    QByteArray decodeTelnet(const QByteArray &input);
    void processLoginBuffer();
    void pollPosition();
    void sendPositionQuery(PositionQuery query);
    void processPositionResponse();
    bool sendLine(const QByteArray &line);
    void sendTelnetResponse(unsigned char response, unsigned char option);
    bool hasCommandPrompt() const;
    void fail(const QString &message);
    void clearCredentials();
    void setState(State state);

    QTcpSocket socket_;
    QTimer *timeoutTimer_;
    QTimer *positionTimer_;
    State state_ = State::Disconnected;
    PositionQuery positionQuery_ = PositionQuery::None;
    double pendingAzimuth_ = 0.0;
    bool positionErrorReported_ = false;
    TelnetParseState telnetParseState_ = TelnetParseState::Data;
    unsigned char telnetOptionCommand_ = 0;
    QByteArray readBuffer_;
    QByteArray username_;
    QByteArray password_;
};

#endif // DISHCLICLIENT_H
