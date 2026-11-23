#include "localipcserver.h"
#include "scancontroller.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

class IpcTest : public QObject
{
    Q_OBJECT

private slots:
    void ipcLossPausesScanButLocalControlsRemainAvailable();
    void localIpcUsesNewlineDelimitedJson();
};

void IpcTest::ipcLossPausesScanButLocalControlsRemainAvailable()
{
    ScanController controller;
    QCOMPARE(controller.state(), ScanController::State::Stopped);

    QVERIFY(controller.startScan());
    QCOMPARE(controller.state(), ScanController::State::Scanning);

    controller.pauseForIpcLoss();
    QCOMPARE(controller.state(), ScanController::State::Paused);

    QVERIFY(controller.startScan());
    QCOMPARE(controller.state(), ScanController::State::Scanning);
    QVERIFY(controller.stopScan());
    QCOMPARE(controller.state(), ScanController::State::Stopped);
}

void IpcTest::localIpcUsesNewlineDelimitedJson()
{
    const QString serverName =
        QStringLiteral("Dominator-test-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    LocalIpcServer server(serverName);
    QVERIFY(server.isListening());

    QSignalSpy startSpy(&server, &LocalIpcServer::startScanRequested);
    QSignalSpy signalSpy(&server, &LocalIpcServer::signalLevelReceived);
    QVERIFY(startSpy.isValid());
    QVERIFY(signalSpy.isValid());

    QLocalSocket client;
    client.connectToServer(serverName);
    QTRY_VERIFY(server.hasClient());
    QTRY_COMPARE(client.state(), QLocalSocket::ConnectedState);

    const QByteArray startMessage = QJsonDocument(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("scan_command")},
        {QStringLiteral("command"), QStringLiteral("start")},
        {QStringLiteral("request_id"), QStringLiteral("request-1")}
    }).toJson(QJsonDocument::Compact);
    QVERIFY(client.write(startMessage) > 0);
    client.flush();
    QCoreApplication::processEvents();
    QCOMPARE(startSpy.count(), 0);

    QVERIFY(client.write("\n") == 1);
    client.flush();
    QTRY_COMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.takeFirst().at(0).toString(), QStringLiteral("request-1"));

    const QByteArray signalMessage = QJsonDocument(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("signal_level")},
        {QStringLiteral("level_dBuV"), 42.5}
    }).toJson(QJsonDocument::Compact) + '\n';
    QVERIFY(client.write(signalMessage) > 0);
    client.flush();
    QTRY_COMPARE(signalSpy.count(), 1);
    QCOMPARE(signalSpy.takeFirst().at(0).toDouble(), 42.5);

    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QVERIFY(server.sendPosition(180.0, 25.0, timestamp));
    QTRY_VERIFY(client.canReadLine());

    QJsonParseError parseError;
    const QJsonDocument positionDocument =
        QJsonDocument::fromJson(client.readLine().trimmed(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(positionDocument.isObject());
    const QJsonObject position = positionDocument.object();
    QCOMPARE(position.value(QStringLiteral("type")).toString(),
             QStringLiteral("position"));
    QCOMPARE(position.value(QStringLiteral("azimuth_deg")).toDouble(), 180.0);
    QCOMPARE(position.value(QStringLiteral("elevation_deg")).toDouble(), 25.0);
    QCOMPARE(position.value(QStringLiteral("reference")).toString(),
             QStringLiteral("true_north"));
}

QTEST_GUILESS_MAIN(IpcTest)

#include "ipc_test.moc"
