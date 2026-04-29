#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace mixxx {

struct SponsorSegment {
    double start;
    double end;
    QString category;
};

struct YouTubeVideoInfo {
    QString id;
    QString title;
    QString uploader;
    int durationSec = 0;
};

/// Native YouTube extractor + downloader. Talks directly to YouTube's
/// InnerTube API (the same endpoints the YouTube Android app uses) so this
/// works identically on desktop and Android — no external `yt-dlp` binary,
/// no Java/JNI bridge, no Termux. Pure Qt over HTTPS.
///
/// The ANDROID InnerTube client returns direct, signature-free stream URLs
/// for adaptive audio formats, so search → resolve → download is just three
/// HTTP requests.
class YouTubeService : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeService(QObject* parent = nullptr);

    /// Run a search; emits searchResultsReady(query, results) on completion.
    /// `cap` is the max number of results returned to the caller.
    void searchVideos(const QString& query, int cap = 25);

    /// Download `videoId` to `cacheDir`. Picks the best audio-only adaptive
    /// stream (typically opus-in-webm or aac-in-m4a), writes it to
    /// `<cacheDir>/<videoId>.<ext>`. Emits downloadFinished(videoId, path)
    /// or downloadFailed(videoId, error).
    void downloadVideo(const QString& videoId, const QString& cacheDir);

    /// Fetch SponsorBlock segments for the given videoId from the public
    /// SponsorBlock API at sponsor.ajay.app.
    void fetchSponsorSegments(const QString& videoId);

  signals:
    void searchResultsReady(const QString& query, const QList<YouTubeVideoInfo>& results);
    void searchFailed(const QString& query, const QString& error);
    void downloadFinished(const QString& videoId, const QString& localPath);
    void downloadFailed(const QString& videoId, const QString& error);
    void sponsorSegmentsFetched(
            const QString& videoId, const QList<SponsorSegment>& segments);

  private:
    /// POST a JSON body to InnerTube `endpoint` (e.g. "search", "player").
    /// Calls `cb` with the parsed JSON on success, or `errCb` with the error.
    void innerTubePost(const QString& endpoint,
            const QByteArray& body,
            std::function<void(const QByteArray&)> cb,
            std::function<void(const QString&)> errCb);

    /// Internal SponsorBlock fetch used to chain "download → fetch → cut".
    /// The callback receives segments (possibly empty on failure).
    void fetchSponsorSegmentsInternal(
            const QString& videoId,
            std::function<void(const QList<SponsorSegment>&)> cb);

    QNetworkAccessManager* m_pNam;
};

} // namespace mixxx
