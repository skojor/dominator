#include "dishsettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class DishSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultsToPort2003();
    void defaultsFrontPageSettings();
    void savesFrontPageSettings();
    void savesConnectionSettings();
};

void DishSettingsTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("DominatorTests"));
    QCoreApplication::setApplicationName(QStringLiteral("DominatorTests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

void DishSettingsTest::defaultsToPort2003()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());

    DishSettings settings;
    QCOMPARE(settings.tcpPort(), 2003);
}

void DishSettingsTest::defaultsFrontPageSettings()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());

    DishSettings settings;
    QCOMPARE(settings.dishHost(), QString());
    QCOMPARE(settings.azimuthStart(), 0.0);
    QCOMPARE(settings.azimuthEnd(), 360.0);
    QCOMPARE(settings.scanSpeed(), 1.0);
    QCOMPARE(settings.elevationMinimum(), 0.0);
    QCOMPARE(settings.elevationMaximum(), 90.0);
}

void DishSettingsTest::savesFrontPageSettings()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());

    DishSettings settings;
    QString error;
    QVERIFY2(settings.saveFrontPageSettings(QStringLiteral("192.0.2.10"),
                                             15.5, 245.0, 2.5, 10.0, 75.0,
                                             error),
             qPrintable(error));

    DishSettings reloadedSettings;
    QCOMPARE(reloadedSettings.dishHost(), QStringLiteral("192.0.2.10"));
    QCOMPARE(reloadedSettings.azimuthStart(), 15.5);
    QCOMPARE(reloadedSettings.azimuthEnd(), 245.0);
    QCOMPARE(reloadedSettings.scanSpeed(), 2.5);
    QCOMPARE(reloadedSettings.elevationMinimum(), 10.0);
    QCOMPARE(reloadedSettings.elevationMaximum(), 75.0);
}

void DishSettingsTest::savesConnectionSettings()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());

    DishSettings settings;
    QString error;
#ifdef Q_OS_WIN
    const QString password = QStringLiteral("test-password");
#else
    const QString password;
#endif
    QVERIFY2(settings.save(2003, QStringLiteral("operator"), password, error),
             qPrintable(error));

    DishSettings reloadedSettings;
    QCOMPARE(reloadedSettings.tcpPort(), 2003);
    QCOMPARE(reloadedSettings.username(), QStringLiteral("operator"));

#ifdef Q_OS_WIN
    QString reloadedPassword;
    QVERIFY2(reloadedSettings.loadPassword(reloadedPassword, error), qPrintable(error));
    QCOMPARE(reloadedPassword, password);
#endif

    QSettings storedSettings;
    const QString storedPassword =
        storedSettings.value(QStringLiteral("dish/passwordProtected")).toString();
#ifdef Q_OS_WIN
    QVERIFY(!storedPassword.contains(password));
#else
    QVERIFY(storedPassword.isEmpty());
#endif
}

QTEST_GUILESS_MAIN(DishSettingsTest)

#include "dishsettings_test.moc"
