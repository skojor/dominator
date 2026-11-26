#include "n2yoclient.h"

#include <QtTest>

class N2yoClientTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsDocumentedPositionsUrl();
    void selectsOnlyCurrentSamples();
    void rejectsWrongSatelliteAndTimestampGaps();
};

void N2yoClientTest::buildsDocumentedPositionsUrl()
{
    const auto url = N2yoClient::positionsUrl(100057, 59.91, -10.73, 42,
                                             QStringLiteral("test/key?"));
    QCOMPARE(url.toEncoded(), QByteArray(
        "https://api.n2yo.com/rest/v1/satellite/positions/100057/59.910000/-10.730000/42.0/300/&apiKey=test%2Fkey%3F"));
}

void N2yoClientTest::selectsOnlyCurrentSamples()
{
    const QByteArray json = R"({
        "info":{"satid":25544,"satname":"SPACE STATION"},
        "positions":[
            {"timestamp":1700000000,"azimuth":359.5,"elevation":-0.1},
            {"timestamp":1700000001,"azimuth":0.5,"elevation":0.1}
        ]
    })";
    SatellitePrediction prediction;
    QString error;
    QVERIFY2(N2yoClient::parsePositions(json, 25544, prediction, error), qPrintable(error));
    QCOMPARE(prediction.name, QStringLiteral("SPACE STATION"));
    QVERIFY(!prediction.pointAt(1699999999));
    QVERIFY(prediction.pointAt(1700000000));
    QCOMPARE(prediction.pointAt(1700000000)->elevation, -0.1);
    QVERIFY(prediction.pointAt(1700000001));
    QCOMPARE(prediction.pointAt(1700000001)->azimuth, 0.5);
    QVERIFY(!prediction.pointAt(1700000002));
}

void N2yoClientTest::rejectsWrongSatelliteAndTimestampGaps()
{
    const QByteArray json = R"({
        "info":{"satid":25544},
        "positions":[
            {"timestamp":1700000000,"azimuth":10,"elevation":5},
            {"timestamp":1700000002,"azimuth":20,"elevation":6}
        ]
    })";
    SatellitePrediction prediction;
    QString error;
    QVERIFY(!N2yoClient::parsePositions(json, 12345, prediction, error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!N2yoClient::parsePositions(json, 25544, prediction, error));
    QVERIFY(!error.isEmpty());
    QVERIFY(prediction.points.isEmpty());
}

QTEST_GUILESS_MAIN(N2yoClientTest)

#include "n2yoclient_test.moc"
