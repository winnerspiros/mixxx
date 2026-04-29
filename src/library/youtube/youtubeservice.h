#pragma once

#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

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

/// Wrapper around the external `yt-dlp` binary plus the SponsorBlock public API.
///
/// The class is intentionally process-based rather than linking a library:
/// `yt-dlp` is a moving target (YouTube breaks extractors regularly), and
/// shipping it as a separately-updateable executable is the standard pattern.
/// On Android we ship the binary inside the APK assets; on desktop we expect
/// it on `PATH` (configurable via `[YouTube]/yt_dlp_path`).
class YouTubeService : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeService(QObject* parent = nullptr);

    /// Override the binary used for yt-dlp invocations. Empty string falls back
    /// to "yt-dlp" on PATH.
    void setYtDlpPath(const QString& path) {
        m_ytDlpPath = path;
    }

    /// Run a search; emits searchResultsReady(query, results) on completion.
    /// Cap is the max number of results requested from yt-dlp.
    void searchVideos(const QString& query, int cap = 25);

    /// Download `videoId` to `cacheDir`. The output is the best available
    /// audio-only stream, transcoded to opus by yt-dlp+ffmpeg. Emits
    /// downloadFinished(videoId, localPath) on success or downloadFailed on
    /// error. Uses --no-playlist and --no-progress for predictable behavior.
    void downloadVideo(const QString& videoId, const QString& cacheDir);

    /// Fetch SponsorBlock segments for the given videoId. Goes to the public
    /// SponsorBlock API at sponsor.ajay.app — no authentication needed.
    void fetchSponsorSegments(const QString& videoId);

  signals:
    void searchResultsReady(const QString& query, const QList<YouTubeVideoInfo>& results);
    void searchFailed(const QString& query, const QString& error);
    void downloadFinished(const QString& videoId, const QString& localPath);
    void downloadFailed(const QString& videoId, const QString& error);
    void sponsorSegmentsFetched(
            const QString& videoId, const QList<SponsorSegment>& segments);

  private:
    QString m_ytDlpPath;
};

} // namespace mixxx
