#include "dishdirectionindicator.h"
#include "elevationindicator.h"

#include <QtTest>

#include <limits>

class DishDirectionIndicatorTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAzimuthToCompassBearing();
    void ignoresNonFiniteAzimuth();
    void representsElevationFromHorizonToZenith();
    void preservesOutOfRangeElevation();
    void ignoresNonFiniteElevation();
};

void DishDirectionIndicatorTest::normalizesAzimuthToCompassBearing()
{
    DishDirectionIndicator indicator;
    QVERIFY(!indicator.hasAzimuth());

    QVERIFY(indicator.setAzimuthDegrees(0.0));
    QCOMPARE(indicator.azimuthDegrees(), 0.0);

    QVERIFY(indicator.setAzimuthDegrees(90.0));
    QCOMPARE(indicator.azimuthDegrees(), 90.0);

    QVERIFY(indicator.setAzimuthDegrees(-90.0));
    QCOMPARE(indicator.azimuthDegrees(), 270.0);

    QVERIFY(indicator.setAzimuthDegrees(360.0));
    QCOMPARE(indicator.azimuthDegrees(), 0.0);
}

void DishDirectionIndicatorTest::ignoresNonFiniteAzimuth()
{
    DishDirectionIndicator indicator;
    QVERIFY(indicator.setAzimuthDegrees(42.0));

    QVERIFY(!indicator.setAzimuthDegrees(std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(indicator.azimuthDegrees(), 42.0);
}

void DishDirectionIndicatorTest::representsElevationFromHorizonToZenith()
{
    ElevationIndicator indicator;
    QVERIFY(!indicator.hasElevation());

    QVERIFY(indicator.setElevationDegrees(0.0));
    QCOMPARE(indicator.elevationDegrees(), 0.0);

    QVERIFY(indicator.setElevationDegrees(45.0));
    QCOMPARE(indicator.elevationDegrees(), 45.0);

    QVERIFY(indicator.setElevationDegrees(90.0));
    QCOMPARE(indicator.elevationDegrees(), 90.0);
}

void DishDirectionIndicatorTest::preservesOutOfRangeElevation()
{
    ElevationIndicator indicator;
    QVERIFY(indicator.setElevationDegrees(95.0));
    QCOMPARE(indicator.elevationDegrees(), 95.0);
}

void DishDirectionIndicatorTest::ignoresNonFiniteElevation()
{
    ElevationIndicator indicator;
    QVERIFY(indicator.setElevationDegrees(42.0));

    QVERIFY(!indicator.setElevationDegrees(std::numeric_limits<double>::infinity()));
    QCOMPARE(indicator.elevationDegrees(), 42.0);
}

QTEST_MAIN(DishDirectionIndicatorTest)

#include "dishdirectionindicator_test.moc"
