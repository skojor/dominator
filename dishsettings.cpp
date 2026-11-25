#include "dishsettings.h"

#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

namespace {
const QString TcpPortKey = QStringLiteral("dish/tcpPort");
const QString UsernameKey = QStringLiteral("dish/username");
const QString PasswordKey = QStringLiteral("dish/passwordProtected");

bool protectPassword(const QString &password, QByteArray &protectedPassword, QString &error)
{
    const QByteArray plainText = password.toUtf8();
    if (plainText.isEmpty()) {
        protectedPassword.clear();
        return true;
    }

#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plainText.constData()));
    input.cbData = static_cast<DWORD>(plainText.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Dominator dish CLI password", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = QObject::tr("Windows could not encrypt the password (error %1).")
                    .arg(GetLastError());
        return false;
    }

    protectedPassword = QByteArray(reinterpret_cast<const char *>(output.pbData),
                                   static_cast<qsizetype>(output.cbData)).toBase64();
    LocalFree(output.pbData);
    return true;
#else
    Q_UNUSED(protectedPassword);
    error = QObject::tr("Secure password storage is only available on Windows.");
    return false;
#endif
}

bool unprotectPassword(const QByteArray &protectedPassword, QString &password, QString &error)
{
    if (protectedPassword.isEmpty()) {
        password.clear();
        return true;
    }

#ifdef Q_OS_WIN
    const QByteArray encrypted = QByteArray::fromBase64(protectedPassword);
    if (encrypted.isEmpty()) {
        error = QObject::tr("The saved password is invalid.");
        return false;
    }

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = QObject::tr("Windows could not decrypt the saved password (error %1).")
                    .arg(GetLastError());
        return false;
    }

    password = QString::fromUtf8(reinterpret_cast<const char *>(output.pbData),
                                 static_cast<qsizetype>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return true;
#else
    Q_UNUSED(password);
    error = QObject::tr("Secure password storage is only available on Windows.");
    return false;
#endif
}
}

DishSettings::DishSettings(QObject *parent)
    : QObject(parent)
    , settings_(new QSettings(this))
{
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
    return unprotectPassword(settings_->value(PasswordKey).toByteArray(), password, error);
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
    if (!protectPassword(password, protectedPassword, error))
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
