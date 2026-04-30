#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

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

/// YouTube extractor + downloader backed by the `yt-dlp` command line tool.
///
/// Why yt-dlp instead of talking to YouTube's InnerTube API directly: Google
/// rotates the Android/iOS client signing schemes, the per-client API keys,
/// and the "PoToken" bot-guard requirement on roughly a quarterly cadence.
/// Anything that hand-rolls the protocol breaks within months. yt-dlp is
/// maintained by a large community that ships fixes within days, so by
/// shelling out to it we trade an extra runtime dependency for a backend
/// that actually keeps working.
///
/// The binary is discovered via (in order): the `MIXXX_YTDLP` env variable,
/// QStandardPaths::findExecutable("yt-dlp"), and a small list of common
/// install locations (/usr/local/bin, /opt/homebrew/bin, …). If none is
/// found, search/download requests fail with a clear "yt-dlp not found"
/// message that the YouTube pane surfaces to the user.
class YouTubeService : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeService(QObject* parent = nullptr);

    /// Run a search; emits searchResultsReady(query, results) on completion.
    /// `cap` is the max number of results returned to the caller.
    void searchVideos(const QString& query, int cap = 25);

    /// Download `videoId` to `cacheDir`. Picks the best audio-only stream
    /// (typically opus-in-webm or aac-in-m4a), writes it to
    /// `<cacheDir>/<videoId>.<ext>`. Emits downloadFinished(videoId, path)
    /// or downloadFailed(videoId, error).
    void downloadVideo(const QString& videoId, const QString& cacheDir);

    /// Fetch SponsorBlock segments for the given videoId from the public
    /// SponsorBlock API at sponsor.ajay.app.
    void fetchSponsorSegments(const QString& videoId);

    /// Absolute path to the yt-dlp binary that will be used, or empty if
    /// none was found. Useful for surfacing setup errors to the user.
    QString ytDlpPath() const {
        return m_ytDlpPath;
    }

  signals:
    void searchResultsReady(const QString& query, const QList<mixxx::YouTubeVideoInfo>& results);
    void searchFailed(const QString& query, const QString& error);
    void downloadFinished(const QString& videoId, const QString& localPath);
    void downloadFailed(const QString& videoId, const QString& error);
    void sponsorSegmentsFetched(
            const QString& videoId, const QList<mixxx::SponsorSegment>& segments);

  private:
    /// Locate the yt-dlp binary. Cached in m_ytDlpPath at construction.
    static QString locateYtDlp();

    /// Spawn yt-dlp with `args`; on completion call `onSuccess(stdout)` or
    /// `onFailure(message)`. `timeoutMs` is enforced via QTimer; processes
    /// that exceed it are killed and surfaced as a failure. The QProcess is
    /// parented to `this` and deletes itself on finish.
    void runYtDlp(const QStringList& args,
            int timeoutMs,
            const std::function<void(const QByteArray&)>& onSuccess,
            const std::function<void(const QString&)>& onFailure);

    /// Internal SponsorBlock fetch used to chain "download → fetch → cut".
    /// The callback receives segments (possibly empty on failure).
    void fetchSponsorSegmentsInternal(
            const QString& videoId,
            const std::function<void(const QList<SponsorSegment>&)>& cb);

    QNetworkAccessManager* m_pNam;
    QString m_ytDlpPath;
};

} // namespace mixxx
