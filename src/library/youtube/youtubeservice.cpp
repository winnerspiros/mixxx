#include "library/youtube/youtubeservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <utility>

#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("YouTubeService");

QString resolveYtDlp(const QString& override) {
    return override.isEmpty() ? QStringLiteral("yt-dlp") : override;
}
} // namespace

YouTubeService::YouTubeService(QObject* parent)
        : QObject(parent) {
}

void YouTubeService::searchVideos(const QString& query, int cap) {
    if (query.trimmed().isEmpty()) {
        Q_EMIT searchResultsReady(query, {});
        return;
    }
    auto* process = new QProcess(this);
    QStringList args;
    args << QStringLiteral("--dump-json")
         << QStringLiteral("--flat-playlist")
         << QStringLiteral("--no-warnings")
         << QStringLiteral("--no-progress")
         << QStringLiteral("--ignore-errors")
         << QStringLiteral("ytsearch%1:%2").arg(cap).arg(query);
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this, process, query](int exitCode, QProcess::ExitStatus status) {
                process->deleteLater();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    const QString err = QString::fromUtf8(process->readAllStandardError());
                    kLogger.warning() << "yt-dlp search failed:" << err;
                    Q_EMIT searchFailed(query, err);
                    return;
                }
                QList<YouTubeVideoInfo> results;
                const QByteArray output = process->readAllStandardOutput();
                // yt-dlp emits one JSON object per line.
                for (const QByteArray& line : output.split('\n')) {
                    if (line.isEmpty()) {
                        continue;
                    }
                    const QJsonDocument doc = QJsonDocument::fromJson(line);
                    if (!doc.isObject()) {
                        continue;
                    }
                    const QJsonObject obj = doc.object();
                    YouTubeVideoInfo info;
                    info.id = obj.value(QStringLiteral("id")).toString();
                    info.title = obj.value(QStringLiteral("title")).toString();
                    info.uploader = obj.value(QStringLiteral("uploader")).toString();
                    info.durationSec = obj.value(QStringLiteral("duration")).toInt();
                    if (!info.id.isEmpty()) {
                        results.append(info);
                    }
                }
                kLogger.info() << "yt-dlp returned" << results.size()
                               << "results for" << query;
                Q_EMIT searchResultsReady(query, results);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, query](QProcess::ProcessError error) {
                Q_UNUSED(error);
                kLogger.warning() << "yt-dlp not available:" << process->errorString();
                Q_EMIT searchFailed(query,
                        tr("yt-dlp is required for YouTube search. "
                           "Install it and ensure it is on PATH."));
            });
    process->start(resolveYtDlp(m_ytDlpPath), args);
}

void YouTubeService::downloadVideo(const QString& videoId, const QString& cacheDir) {
    QDir().mkpath(cacheDir);
    // Output template uses %(ext)s so yt-dlp picks a sane container after
    // post-processing. We keep file naming deterministic so the
    // SponsorBlockController can correlate cached audio with sidecar metadata.
    const QString outputTemplate =
            QDir(cacheDir).filePath(videoId + QStringLiteral(".%(ext)s"));
    auto* process = new QProcess(this);
    QStringList args;
    args << QStringLiteral("--no-playlist")
         << QStringLiteral("--no-warnings")
         << QStringLiteral("--no-progress")
         << QStringLiteral("-x") // extract audio
         << QStringLiteral("--audio-format") << QStringLiteral("opus")
         << QStringLiteral("--audio-quality") << QStringLiteral("0")
         << QStringLiteral("--write-info-json")
         << QStringLiteral("-o") << outputTemplate;
    // Optional belt-and-braces ad/sponsor removal at download time. Off by
    // default because physically removing segments shifts beat positions in
    // the resulting file, which corrupts BPM analysis for music videos. Power
    // users on personal-use installs can opt in for podcast-style content.
    if (m_removeSponsorsAtDownload) {
        args << QStringLiteral("--sponsorblock-remove")
             << QStringLiteral("sponsor,selfpromo,interaction,intro,outro,preview,music_offtopic");
    }
    args << QStringLiteral("https://www.youtube.com/watch?v=") + videoId;
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this, process, videoId, cacheDir](int exitCode, QProcess::ExitStatus status) {
                process->deleteLater();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    const QString err = QString::fromUtf8(process->readAllStandardError());
                    kLogger.warning() << "yt-dlp download failed for" << videoId
                                      << ":" << err;
                    Q_EMIT downloadFailed(videoId, err);
                    return;
                }
                // Find the produced file (extension is determined by yt-dlp).
                const QDir dir(cacheDir);
                const QStringList candidates =
                        dir.entryList({videoId + QStringLiteral(".*")},
                                QDir::Files | QDir::NoDotAndDotDot);
                QString audioPath;
                for (const QString& cand : candidates) {
                    if (cand.endsWith(QStringLiteral(".info.json"))) {
                        continue;
                    }
                    audioPath = dir.filePath(cand);
                    break;
                }
                if (audioPath.isEmpty()) {
                    Q_EMIT downloadFailed(videoId,
                            tr("yt-dlp completed but no output file was found"));
                    return;
                }
                kLogger.info() << "Downloaded" << videoId << "to" << audioPath;
                Q_EMIT downloadFinished(videoId, audioPath);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, videoId](QProcess::ProcessError error) {
                Q_UNUSED(error);
                kLogger.warning() << "yt-dlp launch failed:" << process->errorString();
                Q_EMIT downloadFailed(videoId,
                        tr("yt-dlp could not be launched. Install it and ensure "
                           "it is on PATH."));
            });
    process->start(resolveYtDlp(m_ytDlpPath), args);
}

void YouTubeService::fetchSponsorSegments(const QString& videoId) {
    auto* manager = new QNetworkAccessManager(this);
    // SponsorBlock public API: https://wiki.sponsor.ajay.app/w/API_Docs
    //
    // We request the union of every category that is "ad-like" for a DJ
    // workflow: third-party sponsor reads, creator self-promo, intros/outros,
    // viewer-interaction reminders ("smash that like button"), previews of
    // future content, and non-music sections within music videos.
    //
    // Note on YouTube's served ads (pre-/mid-/post-roll): those are never
    // present in the audio yt-dlp downloads, so there is nothing to block at
    // this layer — the architecture is already ad-free for them.
    const QUrl url(
            QStringLiteral("https://sponsor.ajay.app/api/skipSegments?videoID=%1"
                           "&categories=[\"sponsor\",\"selfpromo\",\"interaction\","
                           "\"intro\",\"outro\",\"preview\",\"music_offtopic\"]")
                    .arg(videoId));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mixxx/SponsorBlock");
    QNetworkReply* reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, videoId, manager]() {
                QList<SponsorSegment> segments;
                if (reply->error() == QNetworkReply::NoError) {
                    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                    const QJsonArray array = doc.array();
                    for (const auto& value : std::as_const(array)) {
                        const QJsonObject obj = value.toObject();
                        const QJsonArray segmentArray =
                                obj[QStringLiteral("segment")].toArray();
                        if (segmentArray.size() < 2) {
                            continue;
                        }
                        segments.append({segmentArray[0].toDouble(),
                                segmentArray[1].toDouble(),
                                obj[QStringLiteral("category")].toString()});
                    }
                } else if (reply->error() != QNetworkReply::ContentNotFoundError) {
                    // 404 just means "no segments for this video" — not worth a warning.
                    kLogger.debug() << "SponsorBlock fetch failed for" << videoId
                                    << ":" << reply->errorString();
                }
                Q_EMIT sponsorSegmentsFetched(videoId, segments);
                reply->deleteLater();
                manager->deleteLater();
            });
}

} // namespace mixxx

#include "moc_youtubeservice.cpp"
