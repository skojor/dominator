#include "dishsettings.h"

#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

namespace {
const QString DishHostKey = QStringLiteral("dish/host");
const QString AzimuthStartKey = QStringLiteral("scan/azimuthStart");
const QString AzimuthEndKey = QStringLiteral("scan/azimuthEnd");
const QString ScanSpeedKey = QStringLiteral("scan/speed");
const QString ElevationMinimumKey = QStringLiteral("scan/elevationMinimum");
const QString ElevationMaximumKey = QStringLiteral("scan/elevationMaximum");
const QString TcpPortKey = QStringLiteral("dish/tcpPort");
const QString UsernameKey = QStringLiteral("dish/username");
const QString PasswordKey = QStringLiteral("dish/passwordProtected");
const QString CatalogIdKey = QStringLiteral("satellitePreview/catalogId");
const QString LatitudeKey = QStringLiteral("satellitePreview/latitude");
const QString LongitudeKey = QStringLiteral("satellitePreview/longitude");
const QString AltitudeKey = QStringLiteral("satellitePreview/altitude");
const QString ApiKeyStorageKey = QStringLiteral("satellitePreview/apiKeyProtected");

bool protectCredential(const QString &credential, const wchar_t *description,
                       QByteArray &protectedCredential, QString &error)
{
    const QByteArray plainText = credential.toUtf8();
    if (plainText.isEmpty()) {
        protectedCredential.clear();
        return true;
    }

#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plainText.constData()));
    input.cbData = static_cast<DWORD>(plainText.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, description, nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = QObject::tr("Windows could not encrypt the credential (error %1).")
                    .arg(GetLastError());
        return false;
    }

    protectedCredential = QByteArray(reinterpret_cast<const char *>(output.pbData),
                                     static_cast<qsizetype>(output.cbData)).toBase64();
    LocalFree(output.pbData);
    return true;
#else
    Q_UNUSED(description);
    error = QObject::tr("Secure credential storage is only available on Windows.");
    return false;
#endif
}

bool unprotectCredential(const QByteArray &protectedCredential, QString &credential, QString &error)
{
    credential.clear();
    if (protectedCredential.isEmpty()) {
        return true;
    }

#ifdef Q_OS_WIN
    const QByteArray encrypted = QByteArray::fromBase64(protectedCredential);
    if (encrypted.isEmpty()) {
        error = QObject::tr("The saved credential is invalid.");
        return false;
    }

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = QObject::tr("Windows could not decrypt the saved credential (error %1).")
                    .arg(GetLastError());
        return false;
    }

    credential = QString::fromUtf8(reinterpret_cast<const char *>(output.pbData),
                                   static_cast<qsizetype>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return true;
#else
    error = QObject::tr("Secure credential storage is only available on Windows.");
    return false;
#endif
}
}

DishSettings::DishSettings(QObject *parent)
    : QObject(parent)
    , settings_(new QSettings(this))
{
}

QString DishSettings::dishHost() const
{
    return settings_->value(DishHostKey).toString();
}

double DishSettings::azimuthStart() const
{
    return settings_->value(AzimuthStartKey, 0.0).toDouble();
}

double DishSettings::azimuthEnd() const
{
    return settings_->value(AzimuthEndKey, 360.0).toDouble();
}

double DishSettings::scanSpeed() const
{
    return settings_->value(ScanSpeedKey, 1.0).toDouble();
}

double DishSettings::elevationMinimum() const
{
    return settings_->value(ElevationMinimumKey, 0.0).toDouble();
}

double DishSettings::elevationMaximum() const
{
    return settings_->value(ElevationMaximumKey, 90.0).toDouble();
}

int DishSettings::tcpPort() const
{
    const int port = settings_->value(TcpPortKey, 2003).toInt();
    return port >= 1 && port <= 65535 ? port : 2003;
}

QString DishSettings::username() const
{
    return settings_->value(UsernameKey).toString();
}

bool DishSettings::loadPassword(QString &password, QString &error) const
{
    return unprotectCredential(settings_->value(PasswordKey).toByteArray(), password, error);
}

SatellitePreviewSettings DishSettings::satellitePreview() const
{
    return {settings_->value(CatalogIdKey).toString(),
            settings_->value(LatitudeKey, 0.0).toDouble(),
            settings_->value(LongitudeKey, 0.0).toDouble(),
            settings_->value(AltitudeKey, 0.0).toDouble()};
}

bool DishSettings::loadN2yoApiKey(QString &apiKey, QString &error) const
{
    return unprotectCredential(settings_->value(ApiKeyStorageKey).toByteArray(), apiKey, error);
}

bool DishSettings::saveSatellitePreview(const SatellitePreviewSettings &preview,
                                        const QString &apiKey, QString &error)
{
    QByteArray protectedKey;
    if (!protectCredential(apiKey, L"Dominator N2YO API key", protectedKey, error))
        return false;

    settings_->setValue(CatalogIdKey, preview.catalogId);
    settings_->setValue(LatitudeKey, preview.latitude);
    settings_->setValue(LongitudeKey, preview.longitude);
    settings_->setValue(AltitudeKey, preview.altitude);
    if (!protectedKey.isEmpty())
        settings_->setValue(ApiKeyStorageKey, protectedKey);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        error = tr("Could not save the preview settings.");
        return false;
    }

    return true;
}

bool DishSettings::saveFrontPageSettings(const QString &dishHost,
                                         double azimuthStart,
                                         double azimuthEnd,
                                         double scanSpeed,
                                         double elevationMinimum,
                                         double elevationMaximum,
                                         QString &error)
{
    settings_->setValue(DishHostKey, dishHost);
    settings_->setValue(AzimuthStartKey, azimuthStart);
    settings_->setValue(AzimuthEndKey, azimuthEnd);
    settings_->setValue(ScanSpeedKey, scanSpeed);
    settings_->setValue(ElevationMinimumKey, elevationMinimum);
    settings_->setValue(ElevationMaximumKey, elevationMaximum);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        error = tr("Could not save front-page settings.");
        return false;
    }

    return true;
}

bool DishSettings::save(int tcpPort,
                        const QString &username,
                        const QString &password,
                        QString &error)
{
    if (tcpPort < 1 || tcpPort > 65535) {
        error = tr("TCP port must be between 1 and 65535.");
        return false;
    }

    QByteArray protectedPassword;
    if (!protectCredential(password, L"Dominator dish CLI password", protectedPassword, error))
        return false;

    settings_->setValue(TcpPortKey, tcpPort);
    settings_->setValue(UsernameKey, username.trimmed());
    settings_->setValue(PasswordKey, protectedPassword);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        error = tr("Could not save dish settings.");
        return false;
    }

    return true;
}
