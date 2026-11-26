#ifndef N2YOCLIENT_H
#define N2YOCLIENT_H

#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>
#include <QVector>

class QNetworkReply;

struct SatellitePointing
{
    qint64 timestamp;
    double azimuth;
    double elevation;
};

struct SatellitePrediction
{
    QString name;
    QVector<SatellitePointing> points;

    const SatellitePointing *pointAt(qint64 timestamp) const;
};

class N2yoClient : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    void fetch(qint64 satelliteId, double latitude, double longitude,
               double altitude, const QString &apiKey);
    void cancel();

    static QUrl positionsUrl(qint64 satelliteId, double latitude, double longitude,
                             double altitude, const QString &apiKey);
    static bool parsePositions(const QByteArray &json, qint64 satelliteId,
                               SatellitePrediction &prediction, QString &error);

signals:
    void predictionReceived(const SatellitePrediction &prediction);
    void requestFailed(const QString &error);

private:
    QNetworkAccessManager network_;
    QNetworkReply *reply_ = nullptr;
};

#endif // N2YOCLIENT_H
