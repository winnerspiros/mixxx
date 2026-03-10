#include "library/youtube/youtubeservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("YouTubeService");
}

YouTubeService::YouTubeService(QObject* parent)
        : QObject(parent) {
}

void YouTubeService::fetchSponsorSegments(const QString& videoId) {
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QUrl url(QString("https://sponsor.ajay.app/api/skipSegments?videoID=%1&categories=[\"sponsor\",\"selfpromo\",\"interaction\",\"intro\",\"outro\",\"preview\",\"music_offtopic\"]")
                     .arg(videoId));

    QNetworkRequest request(url);
    QNetworkReply* reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply, videoId, manager]() {
        QList<SponsorSegment> segments;
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray array = doc.array();
            for (const auto& value : array) {
                QJsonObject obj = value.toObject();
                QJsonArray segmentArray = obj["segment"].toArray();
                segments.append({segmentArray[0].toDouble(),
                        segmentArray[1].toDouble(),
                        obj["category"].toString()});
            }
        }
        emit sponsorSegmentsFetched(videoId, segments);
        reply->deleteLater();
        manager->deleteLater();
    });
}

} // namespace mixxx

#include "moc_youtubeservice.cpp"
