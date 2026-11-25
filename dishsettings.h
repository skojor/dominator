#ifndef DISHSETTINGS_H
#define DISHSETTINGS_H

#include <QObject>
#include <QString>

class QSettings;

class DishSettings : public QObject
{
public:
    explicit DishSettings(QObject *parent = nullptr);

    int tcpPort() const;
    QString username() const;
    bool loadPassword(QString &password, QString &error) const;
    bool save(int tcpPort,
              const QString &username,
              const QString &password,
              QString &error);

private:
    QSettings *settings_;
};

#endif // DISHSETTINGS_H
