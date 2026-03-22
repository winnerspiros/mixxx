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
} // namespace

YouTubeService::YouTubeService(QObject* parent)
        : QObject(parent) {
}

void YouTubeService::fetchSponsorSegments(const QString& videoId) {
    kLogger.info() << "Fetching SponsorBlock segments for:" << videoId;
    QList<SponsorSegment> segments;
    Q_EMIT sponsorSegmentsFetched(videoId, segments);
}

} // namespace mixxx

#include "moc_youtubeservice.cpp"
