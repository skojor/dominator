#include "dishcliclient.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class DishCliClientTest : public QObject
{
    Q_OBJECT

private slots:
    void logsInAndPollsWorldFramePosition();
    void rejectsBlankHostAndUsername();
};

void DishCliClientTest::logsInAndPollsWorldFramePosition()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DishCliClient client;
    QSignalSpy positionSpy(&client, &DishCliClient::positionReceived);
    QSignalSpy unavailableSpy(&client, &DishCliClient::positionUnavailable);
    QSignalSpy errorSpy(&client, &DishCliClient::errorOccurred);
    client.connectToDish(QStringLiteral("127.0.0.1"), server.serverPort(),
                         QStringLiteral("operator"), QStringLiteral("test-password"));
    QTRY_VERIFY(server.hasPendingConnections());

    QTcpSocket *peer = server.nextPendingConnection();
    QVERIFY(peer);
    QByteArray received;
    connect(peer, &QTcpSocket::readyRead, this, [&received, peer] {
        received.append(peer->readAll());
    });

    const char telnetWillEcho[] = {
        static_cast<char>(255), static_cast<char>(251), static_cast<char>(1)
    };
    QByteArray expectedTelnetResponse;
    expectedTelnetResponse.append(static_cast<char>(255));
    expectedTelnetResponse.append(static_cast<char>(254));
    expectedTelnetResponse.append(static_cast<char>(1));
    peer->write(telnetWillEcho, sizeof(telnetWillEcho));
    peer->write("Power On Self Test (POST) Passed.\r\nUser");
    peer->flush();
    QTRY_VERIFY(received.startsWith(expectedTelnetResponse));

    peer->write("name: ");
    peer->flush();
    QTRY_VERIFY(received.contains("operator\r"));

    peer->write("Pass");
    peer->flush();
    peer->write("word: ");
    peer->flush();
    QTRY_VERIFY(received.contains("test-password\r"));

    peer->write("\r\n>");
    peer->flush();
    QTRY_COMPARE(client.state(), DishCliClient::State::Ready);
    QCOMPARE(client.stateText(), QStringLiteral("Connected"));

    const QByteArray azimuthQuery = "show status antenna earth_az\r";
    const QByteArray elevationQuery = "show status antenna earth_el\r";
    QTRY_VERIFY(received.endsWith(azimuthQuery));
    received.clear();
    peer->write("show status antenna earth_az\r\nEARTH_AZ: -0.");
    peer->flush();
    QTest::qWait(50);
    QVERIFY(received.isEmpty());
    peer->write("5\r\n> ");
    peer->flush();
    QTRY_COMPARE(received, elevationQuery);
    QCOMPARE(positionSpy.count(), 0);

    received.clear();
    peer->write("25.25\r\n>");
    peer->flush();
    QTRY_COMPARE(positionSpy.count(), 1);
    QCOMPARE(positionSpy.first().at(0).toDouble(), 359.5);
    QCOMPARE(positionSpy.first().at(1).toDouble(), 25.25);

    QTRY_COMPARE(received, azimuthQuery);
    received.clear();
    peer->write("180\r\n>");
    peer->flush();
    QTRY_COMPARE(received, elevationQuery);
    peer->write("ERROR: invalid command 123\r\n>");
    peer->flush();
    QTRY_COMPARE(unavailableSpy.count(), 1);
    QCOMPARE(positionSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);

    client.disconnectFromDish();
    QCOMPARE(client.state(), DishCliClient::State::Disconnected);
}

void DishCliClientTest::rejectsBlankHostAndUsername()
{
    DishCliClient client;
    QSignalSpy errorSpy(&client, &DishCliClient::errorOccurred);
    QVERIFY(errorSpy.isValid());

    client.connectToDish(QString(), 2003, QStringLiteral("operator"), QString());
    QCOMPARE(client.state(), DishCliClient::State::Disconnected);
    QCOMPARE(errorSpy.count(), 1);

    client.connectToDish(QStringLiteral("127.0.0.1"), 2003, QString(), QString());
    QCOMPARE(client.state(), DishCliClient::State::Disconnected);
    QCOMPARE(errorSpy.count(), 2);
}

QTEST_GUILESS_MAIN(DishCliClientTest)

#include "dishcliclient_test.moc"
