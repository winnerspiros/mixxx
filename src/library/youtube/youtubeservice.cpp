#include "library/youtube/youtubeservice.h"

#include <QByteArray>
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
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <utility>

#include "library/youtube/youtubeaudiocutter.h"
#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("YouTubeService");

// Default per-request budgets. Search is bounded — yt-dlp talks to the
// regular YouTube search endpoint and parses the page. Downloads can be
// large, but bestaudio for a song is typically <10 MB and finishes in
// seconds; we leave a generous ceiling for slow connections / long mixes.
constexpr int kSearchTimeoutMs = 30 * 1000;       // 30 s
constexpr int kDownloadTimeoutMs = 10 * 60 * 1000; // 10 min

// Locations to probe for yt-dlp when it is not on PATH (covers the most
// common installs that QStandardPaths::findExecutable misses on Mac/Linux
// because /opt/homebrew/bin and ~/.local/bin are often outside the GUI app's
// inherited PATH).
const QStringList kFallbackBins = {
        QStringLiteral("/usr/local/bin/yt-dlp"),
        QStringLiteral("/usr/bin/yt-dlp"),
        QStringLiteral("/opt/homebrew/bin/yt-dlp"),
        QStringLiteral("/opt/local/bin/yt-dlp"),
        QStringLiteral("/data/data/com.termux/files/usr/bin/yt-dlp"),
};
} // namespace

YouTubeService::YouTubeService(QObject* parent)
        : QObject(parent),
          m_pNam(new QNetworkAccessManager(this)),
          m_ytDlpPath(locateYtDlp()) {
    if (m_ytDlpPath.isEmpty()) {
        kLogger.warning() << "yt-dlp binary not found. YouTube search and "
                             "download will fail until yt-dlp is installed "
                             "(https://github.com/yt-dlp/yt-dlp) or the "
                             "MIXXX_YTDLP environment variable points at it.";
    } else {
        kLogger.info() << "Using yt-dlp at" << m_ytDlpPath;
    }
}

QString YouTubeService::locateYtDlp() {
    // 1. Explicit override wins. Useful for portable installs and for users
    //    on macOS GUI launches where the inherited PATH lacks /opt/homebrew.
    const QByteArray envPath =
            qgetenv("MIXXX_YTDLP");
    if (!envPath.isEmpty()) {
        const QString p = QString::fromLocal8Bit(envPath);
        if (QFileInfo(p).isExecutable()) {
            return p;
        }
        kLogger.warning() << "MIXXX_YTDLP set to" << p
                          << "but that path is not executable; falling back to PATH";
    }
    // 2. PATH lookup (handles "yt-dlp" and "yt-dlp.exe" on Windows).
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    // 3. Common install locations not always in $PATH for GUI apps.
    for (const QString& candidate : kFallbackBins) {
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    return QString();
}

void YouTubeService::runYtDlp(const QStringList& args,
        int timeoutMs,
        const std::function<void(const QByteArray&)>& onSuccess,
        const std::function<void(const QString&)>& onFailure) {
    if (m_ytDlpPath.isEmpty()) {
        onFailure(tr("yt-dlp is not installed. Install it from "
                     "https://github.com/yt-dlp/yt-dlp (or `pip install yt-dlp`) "
                     "and restart Mixxx. You can also set the MIXXX_YTDLP "
                     "environment variable to a custom binary path."));
        return;
    }

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    // Watchdog timer — yt-dlp normally exits on its own well before this,
    // but a hung download or DNS stall would otherwise block the user
    // forever. Killing the process triggers `finished`, which the lambda
    // below converts into a user-visible error.
    //
    // The timer is parented to `proc`, so when `proc` is destroyed the
    // timer is destroyed with it (preventing any chance of a fire after
    // proc tear-down). We additionally stop() it explicitly in the
    // finished/errorOccurred handlers so it can't race deleteLater().
    auto* watchdog = new QTimer(proc);
    watchdog->setSingleShot(true);
    watchdog->setInterval(timeoutMs);
    connect(watchdog, &QTimer::timeout, proc, [proc]() {
        kLogger.warning() << "yt-dlp watchdog timeout — killing process";
        proc->kill();
    });

    // Cleanup is centralized: `finished` is the single place that calls
    // deleteLater(). FailedToStart is the one error case where `finished`
    // does *not* fire (Qt skips it because the process never started), so
    // the errorOccurred handler synthesizes a failure for that one case
    // only and routes it through the same callback.
    connect(proc,
            &QProcess::finished,
            this,
            [proc, watchdog, onSuccess, onFailure](
                    int exitCode, QProcess::ExitStatus status) {
                watchdog->stop();
                proc->deleteLater();
                const QByteArray out = proc->readAllStandardOutput();
                const QByteArray err = proc->readAllStandardError();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    QString msg = QString::fromLocal8Bit(err).trimmed();
                    if (msg.isEmpty()) {
                        msg = tr("yt-dlp exited with code %1").arg(exitCode);
                    }
                    onFailure(msg);
                    return;
                }
                onSuccess(out);
            });
    connect(proc,
            &QProcess::errorOccurred,
            this,
            [proc, watchdog, onFailure](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    // Crashed / Timedout / WriteError / ReadError / Unknown
                    // are all followed by `finished`, which owns cleanup.
                    return;
                }
                watchdog->stop();
                proc->deleteLater();
                onFailure(tr("Failed to launch yt-dlp"));
            });

    watchdog->start();
    proc->start(m_ytDlpPath, args);
}

void YouTubeService::searchVideos(const QString& query, int cap) {
    if (query.trimmed().isEmpty()) {
        Q_EMIT searchResultsReady(query, {});
        return;
    }
    // --flat-playlist makes yt-dlp return only metadata for each result
    // (no per-video page fetch), which keeps the round-trip under a second
    // and is enough for id/title/uploader/duration. --skip-download is
    // implied by the dump but kept explicit for safety.
    const QStringList args = {
            QStringLiteral("--flat-playlist"),
            QStringLiteral("--skip-download"),
            QStringLiteral("--dump-single-json"),
            QStringLiteral("--no-warnings"),
            QStringLiteral("--no-cache-dir"),
            QStringLiteral("--ignore-config"),
            QStringLiteral("--default-search"),
            QStringLiteral("ytsearch"),
            QStringLiteral("ytsearch%1:%2").arg(cap).arg(query),
    };
    runYtDlp(
            args,
            kSearchTimeoutMs,
            [this, query, cap](const QByteArray& stdoutBytes) {
                const QJsonDocument doc = QJsonDocument::fromJson(stdoutBytes);
                const QJsonObject root = doc.object();
                const QJsonArray entries =
                        root.value(QStringLiteral("entries")).toArray();
                QList<YouTubeVideoInfo> results;
                results.reserve(entries.size());
                for (const auto& v : entries) {
                    if (results.size() >= cap) {
                        break;
                    }
                    const QJsonObject entry = v.toObject();
                    YouTubeVideoInfo info;
                    info.id = entry.value(QStringLiteral("id")).toString();
                    info.title =
                            entry.value(QStringLiteral("title")).toString();
                    // yt-dlp populates "channel" for most extractors and
                    // "uploader" as a fallback; prefer channel as it is
                    // closer to the user-visible "Artist".
                    info.uploader =
                            entry.value(QStringLiteral("channel")).toString();
                    if (info.uploader.isEmpty()) {
                        info.uploader = entry.value(QStringLiteral("uploader"))
                                                .toString();
                    }
                    // duration is a JSON number (seconds) for video entries
                    // and may be null for channels/playlists/live streams.
                    const QJsonValue dur = entry.value(QStringLiteral("duration"));
                    if (dur.isDouble()) {
                        info.durationSec = static_cast<int>(dur.toDouble());
                    }
                    if (!info.id.isEmpty() && !info.title.isEmpty()) {
                        results.append(info);
                    }
                }
                kLogger.info() << "yt-dlp returned" << results.size()
                               << "results for" << query;
                Q_EMIT searchResultsReady(query, results);
            },
            [this, query](const QString& err) {
                kLogger.warning() << "yt-dlp search failed:" << err;
                Q_EMIT searchFailed(query, err);
            });
}

void YouTubeService::downloadVideo(const QString& videoId, const QString& cacheDir) {
    QDir().mkpath(cacheDir);
    const QString outTemplate =
            QDir(cacheDir).filePath(QStringLiteral("%(id)s.%(ext)s"));
    // -f bestaudio: pick the best audio-only stream without re-encoding.
    // --no-playlist: defensive — guard against playlist URLs being passed.
    // --print after_move:filepath: emit the final on-disk path on stdout
    //   *after* yt-dlp's atomic .part rename, so we read it deterministically.
    // -- separates options from the videoId so an id like "-abc123" cannot
    //   be misparsed as a flag.
    const QStringList args = {
            QStringLiteral("-f"),
            QStringLiteral("bestaudio"),
            QStringLiteral("--no-playlist"),
            QStringLiteral("--no-warnings"),
            QStringLiteral("--no-progress"),
            QStringLiteral("--no-cache-dir"),
            QStringLiteral("--ignore-config"),
            QStringLiteral("--no-mtime"),
            QStringLiteral("-o"),
            outTemplate,
            QStringLiteral("--print"),
            QStringLiteral("after_move:filepath"),
            QStringLiteral("--"),
            QStringLiteral("https://www.youtube.com/watch?v=") + videoId,
    };
    runYtDlp(
            args,
            kDownloadTimeoutMs,
            [this, videoId, cacheDir](const QByteArray& stdoutBytes) {
                // The last non-empty stdout line is the final filepath
                // (--print after_move:filepath). Earlier lines may include
                // diagnostic noise from extractors; we ignore them.
                QString outPath;
                const QList<QByteArray> lines = stdoutBytes.split('\n');
                for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
                    const QString line = QString::fromLocal8Bit(*it).trimmed();
                    if (!line.isEmpty() && QFileInfo::exists(line)) {
                        outPath = line;
                        break;
                    }
                }
                // Fallback: scan the cache dir for any file matching the id.
                // Covers older yt-dlp versions that don't honor `after_move:`.
                if (outPath.isEmpty()) {
                    const QDir dir(cacheDir);
                    const QStringList existing =
                            dir.entryList({videoId + QStringLiteral(".*")},
                                    QDir::Files | QDir::NoDotAndDotDot);
                    for (const QString& f : existing) {
                        if (f.endsWith(QStringLiteral(".info.json")) ||
                                f.endsWith(QStringLiteral(".sponsor.json")) ||
                                f.endsWith(QStringLiteral(".part"))) {
                            continue;
                        }
                        outPath = dir.filePath(f);
                        break;
                    }
                }
                if (outPath.isEmpty()) {
                    Q_EMIT downloadFailed(videoId,
                            tr("yt-dlp finished but no output file was found"));
                    return;
                }
                kLogger.info() << "Downloaded" << videoId << "→" << outPath;

                // Chain: fetch SponsorBlock segments, physically cut them
                // out of the file (so duration / BPM / waveform all reflect
                // the music-only length), THEN tell consumers the file is
                // ready. We deliberately do this ourselves rather than via
                // yt-dlp's --sponsorblock-remove because the latter requires
                // a re-encode pass through ffmpeg and can drop quality.
                fetchSponsorSegmentsInternal(videoId,
                        [this, videoId, outPath](
                                const QList<SponsorSegment>& segments) {
                            if (!segments.isEmpty()) {
                                const bool cut =
                                        cutAudioRanges(outPath, segments);
                                if (!cut) {
                                    // Cutting failed — write the sidecar so
                                    // SponsorBlockController can fall back
                                    // to skip-at-playback.
                                    QFile sidecar(outPath +
                                            QStringLiteral(".sponsor.json"));
                                    if (sidecar.open(QIODevice::WriteOnly |
                                                QIODevice::Truncate)) {
                                        QJsonArray arr;
                                        for (const auto& s : segments) {
                                            QJsonObject o;
                                            o.insert(QStringLiteral("start"),
                                                    s.start);
                                            o.insert(QStringLiteral("end"),
                                                    s.end);
                                            o.insert(QStringLiteral("category"),
                                                    s.category);
                                            arr.append(o);
                                        }
                                        sidecar.write(
                                                QJsonDocument(arr).toJson(
                                                        QJsonDocument::
                                                                Compact));
                                    }
                                }
                            }
                            Q_EMIT downloadFinished(videoId, outPath);
                        });
            },
            [this, videoId](const QString& err) {
                kLogger.warning() << "yt-dlp download failed:" << err;
                Q_EMIT downloadFailed(videoId, err);
            });
}

void YouTubeService::fetchSponsorSegments(const QString& videoId) {
    fetchSponsorSegmentsInternal(videoId,
            [this, videoId](const QList<SponsorSegment>& segments) {
                Q_EMIT sponsorSegmentsFetched(videoId, segments);
            });
}

void YouTubeService::fetchSponsorSegmentsInternal(
        const QString& videoId,
        const std::function<void(const QList<SponsorSegment>&)>& cb) {
    // SponsorBlock public API: https://wiki.sponsor.ajay.app/w/API_Docs
    // Categories include third-party sponsorships, creator self-promo,
    // intros/outros, viewer-interaction reminders, content previews, and
    // non-music sections within music videos.
    // YouTube-served pre/mid/post-roll ads are never present in the audio we
    // download, so there is nothing to block at this layer.
    const QUrl url(
            QStringLiteral("https://sponsor.ajay.app/api/skipSegments?videoID=%1"
                           "&categories=[\"sponsor\",\"selfpromo\",\"interaction\","
                           "\"intro\",\"outro\",\"preview\",\"music_offtopic\"]")
                    .arg(videoId));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "DJSugar/SponsorBlock");
    QNetworkReply* reply = m_pNam->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, videoId, cb]() {
        reply->deleteLater();
        QList<SponsorSegment> segments;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
            for (const auto& value : array) {
                const QJsonObject obj = value.toObject();
                const QJsonArray seg = obj.value(QStringLiteral("segment")).toArray();
                if (seg.size() < 2) {
                    continue;
                }
                segments.append({seg[0].toDouble(),
                        seg[1].toDouble(),
                        obj.value(QStringLiteral("category")).toString()});
            }
        }
        // 404 just means "no segments for this video" — not an error.
        cb(segments);
    });
}

} // namespace mixxx

#include "moc_youtubeservice.cpp"
