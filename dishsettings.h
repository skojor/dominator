#ifndef DISHSETTINGS_H
#define DISHSETTINGS_H

#include <QObject>
#include <QString>

class QSettings;

struct SatellitePreviewSettings
{
    QString catalogId;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
};

class DishSettings : public QObject
{
public:
    explicit DishSettings(QObject *parent = nullptr);

    QString dishHost() const;
    double azimuthStart() const;
    double azimuthEnd() const;
    double scanSpeed() const;
    double elevationMinimum() const;
    double elevationMaximum() const;
    int tcpPort() const;
    QString username() const;
    bool loadPassword(QString &password, QString &error) const;
    SatellitePreviewSettings satellitePreview() const;
    bool loadN2yoApiKey(QString &apiKey, QString &error) const;
    bool saveSatellitePreview(const SatellitePreviewSettings &preview,
                              const QString &apiKey, QString &error);
    bool saveFrontPageSettings(const QString &dishHost,
                               double azimuthStart,
                               double azimuthEnd,
                               double scanSpeed,
                               double elevationMinimum,
                               double elevationMaximum,
                               QString &error);
    bool save(int tcpPort,
              const QString &username,
              const QString &password,
              QString &error);

private:
    QSettings *settings_;
};

#endif // DISHSETTINGS_H
