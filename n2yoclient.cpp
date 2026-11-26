#include "n2yoclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <utility>

namespace {
constexpr int BatchSeconds = 300;
constexpr int MaxResponseBytes = 256 * 1024;
}

const SatellitePointing *SatellitePrediction::pointAt(qint64 timestamp) const
{
    if (points.isEmpty() || timestamp < points.first().timestamp
        || timestamp > points.last().timestamp) {
        return nullptr;
    }

    const auto &point = points.at(timestamp - points.first().timestamp);
    return point.timestamp == timestamp ? &point : nullptr;
}

QUrl N2yoClient::positionsUrl(qint64 satelliteId, double latitude, double longitude,
                              double altitude, const QString &apiKey)
{
    const auto path = QStringLiteral(
        "https://api.n2yo.com/rest/v1/satellite/positions/%1/%2/%3/%4/%5/&apiKey=")
        .arg(satelliteId)
        .arg(latitude, 0, 'f', 6)
        .arg(longitude, 0, 'f', 6)
        .arg(altitude, 0, 'f', 1)
        .arg(BatchSeconds);
    return QUrl::fromEncoded(path.toLatin1() + QUrl::toPercentEncoding(apiKey));
}

bool N2yoClient::parsePositions(const QByteArray &json, qint64 satelliteId,
                                SatellitePrediction &prediction, QString &error)
{
    const auto root = QJsonDocument::fromJson(json).object();
    const auto info = root.value(QStringLiteral("info")).toObject();
    const auto positions = root.value(QStringLiteral("positions")).toArray();
    if (info.value(QStringLiteral("satid")).toInteger() != satelliteId
        || positions.isEmpty() || positions.size() > BatchSeconds) {
        error = tr("N2YO did not return valid positions for this satellite. Check the catalog ID and API key.");
        return false;
    }

    SatellitePrediction parsed;
    parsed.name = info.value(QStringLiteral("satname")).toString();
    parsed.points.reserve(positions.size());
    for (const auto &entry : positions) {
        const auto position = entry.toObject();
        const auto timestamp = position.value(QStringLiteral("timestamp")).toInteger();
        const auto azimuth = position.value(QStringLiteral("azimuth")).toDouble(-1);
        const auto elevation = position.value(QStringLiteral("elevation")).toDouble(-100);
        if (timestamp <= 0 || timestamp > 253402300799LL
            || !(azimuth >= 0 && azimuth <= 360)
            || !(elevation >= -90 && elevation <= 90)
            || (!parsed.points.isEmpty() && timestamp != parsed.points.last().timestamp + 1)) {
            error = tr("N2YO returned an invalid pointing sample or a gap in its timestamps.");
            return false;
        }
        parsed.points.append({timestamp, azimuth == 360 ? 0 : azimuth, elevation});
    }

    prediction = std::move(parsed);
    return true;
}

void N2yoClient::fetch(qint64 satelliteId, double latitude, double longitude,
                       double altitude, const QString &apiKey)
{
    cancel();
    QNetworkRequest request(positionsUrl(satelliteId, latitude, longitude, altitude, apiKey));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10000);
    auto *reply = network_.get(request);
    reply_ = reply;
    QTimer::singleShot(15000, reply, [reply] {
        if (reply->isRunning())
            reply->abort();
    });
    reply->setReadBufferSize(MaxResponseBytes + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > MaxResponseBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, satelliteId] {
        reply_ = nullptr;
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401 || status == 403) {
            emit requestFailed(tr("N2YO rejected the request. Check the API key."));
        } else if (status == 429) {
            emit requestFailed(tr("N2YO rate limit reached."));
        } else if (status != 0 && status != 200) {
            emit requestFailed(tr("N2YO request failed (HTTP %1).").arg(status));
        } else if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(tr("Could not retrieve N2YO predictions. Check the internet connection and try again."));
        } else {
            SatellitePrediction prediction;
            QString error;
            if (parsePositions(reply->readAll(), satelliteId, prediction, error))
                emit predictionReceived(prediction);
            else
                emit requestFailed(error);
        }
    });
}

void N2yoClient::cancel()
{
    if (!reply_)
        return;

    auto *reply = reply_;
    reply_ = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}
