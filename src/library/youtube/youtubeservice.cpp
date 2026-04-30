#include "library/youtube/youtubeservice.h"

#include <QByteArray>
#include <QCoreApplication>
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
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <utility>

#include "library/youtube/youtubeaudiocutter.h"
#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("YouTubeService");

// Per-request budgets. Search is bounded — Piped's /search and yt-dlp's
// page-parse both finish well under a second on a healthy network. Downloads
// can be larger, but bestaudio for a song is typically <10 MB; the ceiling
// is generous so a slow connection or long mix doesn't get killed.
constexpr int kSearchTimeoutMs = 30 * 1000;        // 30 s
constexpr int kDownloadTimeoutMs = 10 * 60 * 1000; // 10 min
// Piped HTTP requests get a tighter ceiling — each instance failure should
// surface fast so we can fail over to the next one without burning a minute
// per dead instance. /streams is included because it can wait on YouTube
// upstream, but we still want to move on within ~15 s.
constexpr int kPipedHttpTimeoutMs = 15 * 1000;

// Hardcoded list of Piped API instances tried in order on per-request
// failure. Picked from the official Piped instance list
// (https://github.com/TeamPiped/documentation/blob/main/content/docs/public-instances/index.md)
// for geographic spread + uptime track record. Keep the list short — every
// extra instance only adds latency on full-failure paths.
const QStringList kPipedInstances = {
        QStringLiteral("https://api.piped.private.coffee"),
        QStringLiteral("https://pipedapi.kavin.rocks"),
        QStringLiteral("https://api.piped.projectsegfau.lt"),
        QStringLiteral("https://pipedapi.adminforge.de"),
        QStringLiteral("https://pipedapi.r4fo.com"),
};

// Locations to probe for yt-dlp when it is not on PATH. Order matters:
// the bundled binary next to the Mixxx executable wins, then the user's
// PATH, then common install dirs that GUI launches often miss because
// /opt/homebrew/bin and ~/.local/bin are outside the inherited PATH.
QStringList ytDlpFallbackBins() {
    QStringList bins;
#if defined(Q_OS_WIN)
    const QString exe = QStringLiteral("yt-dlp.exe");
#else
    const QString exe = QStringLiteral("yt-dlp");
#endif
    // 1. Bundled next to the Mixxx executable (Win/Mac/Linux desktop installs
    //    ship the official self-contained PyInstaller binary here — see
    //    cmake/modules/FetchYtDlp.cmake).
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        bins << QDir(appDir).filePath(exe);
#if defined(Q_OS_MAC)
        // .app bundle layout: yt-dlp lives in Contents/MacOS alongside the
        // Mixxx binary, but applicationDirPath() already returns that path.
        // Also probe Contents/Resources just in case.
        bins << QDir(appDir).filePath(QStringLiteral("../Resources/") + exe);
#endif
    }
    // 2. Common system locations.
    bins << QStringLiteral("/usr/local/bin/yt-dlp")
         << QStringLiteral("/usr/bin/yt-dlp")
         << QStringLiteral("/opt/homebrew/bin/yt-dlp")
         << QStringLiteral("/opt/local/bin/yt-dlp")
         // Termux-on-Android: only path that ever yields a working yt-dlp
         // on Android, and only if the user has installed it themselves.
         // Piped is the primary Android backend so this is purely a bonus.
         << QStringLiteral("/data/data/com.termux/files/usr/bin/yt-dlp");
    return bins;
}

// Map a Piped audio stream's ext-hint (mimeType) to a sensible file
// extension for the downloaded blob. We name files <id>.<ext> so the rest
// of YouTubeFeature (and the SoundSource picker) can identify them.
QString extFromMime(const QString& mime, const QString& codec) {
    const QString m = mime.toLower();
    if (m.contains(QStringLiteral("webm"))) {
        return QStringLiteral("webm");
    }
    if (m.contains(QStringLiteral("mp4")) || m.contains(QStringLiteral("m4a"))) {
        return QStringLiteral("m4a");
    }
    if (codec.startsWith(QStringLiteral("opus"))) {
        return QStringLiteral("webm");
    }
    if (codec.startsWith(QStringLiteral("mp4a"))) {
        return QStringLiteral("m4a");
    }
    return QStringLiteral("m4a"); // safe default — FFmpeg SoundSource handles it
}
} // namespace

YouTubeService::YouTubeService(QObject* parent)
        : QObject(parent),
          m_pNam(new QNetworkAccessManager(this)),
          m_ytDlpPath(locateYtDlp()),
          m_pipedInstances(kPipedInstances) {
    if (!m_ytDlpPath.isEmpty()) {
        kLogger.info() << "yt-dlp fallback available at" << m_ytDlpPath;
    } else {
        kLogger.info() << "yt-dlp not found; YouTube tab will rely on Piped only "
                          "(this is normal on Android and minimal desktop installs)";
    }
}

QString YouTubeService::locateYtDlp() {
    // 1. Explicit override — useful for portable installs and for users on
    //    macOS GUI launches where the inherited PATH lacks /opt/homebrew.
    const QByteArray envPath = qgetenv("MIXXX_YTDLP");
    if (!envPath.isEmpty()) {
        const QString p = QString::fromLocal8Bit(envPath);
        if (QFileInfo(p).isExecutable()) {
            return p;
        }
        kLogger.warning() << "MIXXX_YTDLP set to" << p
                          << "but that path is not executable; ignoring";
    }
    // 2. Bundled-next-to-binary + common install dirs.
    for (const QString& candidate : ytDlpFallbackBins()) {
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    // 3. PATH lookup — handles "yt-dlp" and "yt-dlp.exe" on Windows.
    const QString fromPath =
            QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    return QString();
}

// =============================================================================
// Public API — picks Piped first, yt-dlp as desktop fallback
// =============================================================================

void YouTubeService::searchVideos(const QString& query, int cap) {
    if (query.trimmed().isEmpty()) {
        Q_EMIT searchResultsReady(query, {});
        return;
    }
    const bool hasYtDlpFallback = !m_ytDlpPath.isEmpty();
    searchViaPiped(query,
            cap,
            /*instanceIdx=*/0,
            [this, query, cap, hasYtDlpFallback](const QString& lastError) {
                if (hasYtDlpFallback) {
                    kLogger.warning() << "All Piped instances failed for search"
                                      << query << ":" << lastError
                                      << "— falling back to yt-dlp";
                    searchViaYtDlp(query, cap);
                } else {
                    kLogger.warning() << "All Piped instances failed for search"
                                      << query << ":" << lastError;
                    Q_EMIT searchFailed(query, lastError);
                }
            });
}

void YouTubeService::downloadVideo(const QString& videoId, const QString& cacheDir) {
    QDir().mkpath(cacheDir);
    const bool hasYtDlpFallback = !m_ytDlpPath.isEmpty();
    downloadViaPiped(videoId,
            cacheDir,
            /*instanceIdx=*/0,
            [this, videoId, cacheDir, hasYtDlpFallback](const QString& lastError) {
                if (hasYtDlpFallback) {
                    kLogger.warning() << "All Piped instances failed for download"
                                      << videoId << ":" << lastError
                                      << "— falling back to yt-dlp";
                    downloadViaYtDlp(videoId, cacheDir);
                } else {
                    kLogger.warning() << "All Piped instances failed for download"
                                      << videoId << ":" << lastError;
                    Q_EMIT downloadFailed(videoId, lastError);
                }
            });
}

// =============================================================================
// Piped (primary backend, works on every Qt platform incl. Android)
// =============================================================================

void YouTubeService::searchViaPiped(const QString& query,
        int cap,
        int instanceIdx,
        const std::function<void(const QString&)>& onAllFailed) {
    if (instanceIdx >= m_pipedInstances.size()) {
        onAllFailed(tr("All Piped instances failed (network or upstream YouTube error)"));
        return;
    }
    const QString instance = m_pipedInstances.at(instanceIdx);
    QUrl url(instance + QStringLiteral("/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("filter"), QStringLiteral("videos"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Mixxx/YouTube");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(kPipedHttpTimeoutMs);

    QNetworkReply* reply = m_pNam->get(req);
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, query, cap, instanceIdx, instance, onAllFailed]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    kLogger.info() << "Piped search via" << instance << "failed:"
                                   << reply->errorString() << "— trying next";
                    searchViaPiped(query, cap, instanceIdx + 1, onAllFailed);
                    return;
                }
                const QJsonDocument doc =
                        QJsonDocument::fromJson(reply->readAll());
                const QJsonArray items =
                        doc.object().value(QStringLiteral("items")).toArray();
                QList<YouTubeVideoInfo> results;
                results.reserve(items.size());
                for (const QJsonValue& v : items) {
                    if (results.size() >= cap) {
                        break;
                    }
                    const QJsonObject obj = v.toObject();
                    // Piped marks non-video items (channels, playlists) with a
                    // different "type"; skip anything that isn't a stream.
                    const QString type = obj.value(QStringLiteral("type")).toString();
                    if (!type.isEmpty() && type != QStringLiteral("stream")) {
                        continue;
                    }
                    YouTubeVideoInfo info;
                    // "url" is a relative "/watch?v=ID" path. Strip the prefix
                    // to recover the bare videoId for /streams/<id> lookup.
                    const QString relUrl =
                            obj.value(QStringLiteral("url")).toString();
                    const int eq = relUrl.indexOf(QLatin1Char('='));
                    if (eq > 0 && eq + 1 < relUrl.size()) {
                        info.id = relUrl.mid(eq + 1);
                    }
                    info.title = obj.value(QStringLiteral("title")).toString();
                    info.uploader =
                            obj.value(QStringLiteral("uploaderName")).toString();
                    const QJsonValue dur =
                            obj.value(QStringLiteral("duration"));
                    if (dur.isDouble()) {
                        info.durationSec = static_cast<int>(dur.toDouble());
                    }
                    if (!info.id.isEmpty() && !info.title.isEmpty()) {
                        results.append(info);
                    }
                }
                if (results.isEmpty()) {
                    // Empty result set is a legitimate "no matches" answer
                    // for a unique query, but for popular queries it almost
                    // always means the instance is degraded. Try the next.
                    kLogger.info() << "Piped instance" << instance
                                   << "returned 0 results for" << query
                                   << "— trying next";
                    searchViaPiped(query, cap, instanceIdx + 1, onAllFailed);
                    return;
                }
                kLogger.info() << "Piped (" << instance << ") returned"
                               << results.size() << "results for" << query;
                Q_EMIT searchResultsReady(query, results);
            });
}

void YouTubeService::downloadViaPiped(const QString& videoId,
        const QString& cacheDir,
        int instanceIdx,
        const std::function<void(const QString&)>& onAllFailed) {
    if (instanceIdx >= m_pipedInstances.size()) {
        onAllFailed(tr("All Piped instances failed for video %1").arg(videoId));
        return;
    }
    const QString instance = m_pipedInstances.at(instanceIdx);
    const QUrl url(instance + QStringLiteral("/streams/") + videoId);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Mixxx/YouTube");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(kPipedHttpTimeoutMs);

    QNetworkReply* reply = m_pNam->get(req);
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, videoId, cacheDir, instanceIdx, instance, onAllFailed]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    kLogger.info() << "Piped /streams via" << instance << "failed:"
                                   << reply->errorString() << "— trying next";
                    downloadViaPiped(
                            videoId, cacheDir, instanceIdx + 1, onAllFailed);
                    return;
                }
                const QJsonObject root =
                        QJsonDocument::fromJson(reply->readAll()).object();
                const QJsonArray audio =
                        root.value(QStringLiteral("audioStreams")).toArray();
                if (audio.isEmpty()) {
                    kLogger.info() << "Piped instance" << instance
                                   << "returned no audioStreams for" << videoId
                                   << "— trying next";
                    downloadViaPiped(
                            videoId, cacheDir, instanceIdx + 1, onAllFailed);
                    return;
                }
                downloadAudioStream(videoId,
                        cacheDir,
                        audio,
                        [this, videoId, cacheDir, instanceIdx, onAllFailed](
                                const QString& err) {
                            kLogger.info() << "Piped audio download failed:"
                                           << err << "— trying next instance";
                            downloadViaPiped(videoId,
                                    cacheDir,
                                    instanceIdx + 1,
                                    onAllFailed);
                        });
            });
}

void YouTubeService::downloadAudioStream(const QString& videoId,
        const QString& cacheDir,
        const QJsonArray& audioStreams,
        const std::function<void(const QString&)>& onFailure) {
    // Pick the highest-bitrate stream. Prefer opus (better quality per bit)
    // when bitrates are tied or close, since Piped sometimes lists a low
    // bitrate AAC alongside high bitrate opus.
    QJsonObject best;
    int bestBitrate = -1;
    bool bestIsOpus = false;
    for (const QJsonValue& v : audioStreams) {
        const QJsonObject s = v.toObject();
        const int br = s.value(QStringLiteral("bitrate")).toInt();
        const QString codec =
                s.value(QStringLiteral("codec")).toString().toLower();
        const bool isOpus = codec.startsWith(QStringLiteral("opus"));
        // Prefer higher bitrate; on a tie prefer opus.
        if (br > bestBitrate ||
                (br == bestBitrate && isOpus && !bestIsOpus)) {
            best = s;
            bestBitrate = br;
            bestIsOpus = isOpus;
        }
    }
    const QString streamUrl = best.value(QStringLiteral("url")).toString();
    if (streamUrl.isEmpty()) {
        onFailure(tr("No usable audio stream URL"));
        return;
    }
    const QString ext = extFromMime(
            best.value(QStringLiteral("mimeType")).toString(),
            best.value(QStringLiteral("codec")).toString().toLower());
    const QString outPath =
            QDir(cacheDir).filePath(videoId + QLatin1Char('.') + ext);

    // Stream straight to disk via a temp file we rename on completion. A
    // partial file left behind from a kill/crash would otherwise look like
    // a complete download to the next launch's library scanner.
    auto* outFile = new QFile(outPath + QStringLiteral(".part"));
    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = outFile->errorString();
        delete outFile;
        onFailure(tr("Cannot open %1: %2").arg(outPath, err));
        return;
    }

    QNetworkRequest req((QUrl(streamUrl)));
    req.setRawHeader("User-Agent", "Mixxx/YouTube");
    // googlevideo CDN is generally fast; a transfer timeout of 10 min mirrors
    // the yt-dlp watchdog and matches kDownloadTimeoutMs.
    req.setTransferTimeout(kDownloadTimeoutMs);

    QNetworkReply* reply = m_pNam->get(req);

    // Stream the body to disk as it arrives — important on Android where
    // RAM-buffering a full audio file can OOM on large mixes.
    connect(reply, &QNetworkReply::readyRead, this, [reply, outFile]() {
        outFile->write(reply->readAll());
    });
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, outFile, outPath, videoId, onFailure]() {
                reply->deleteLater();
                outFile->write(reply->readAll());
                outFile->close();
                if (reply->error() != QNetworkReply::NoError) {
                    QFile::remove(outFile->fileName());
                    delete outFile;
                    onFailure(reply->errorString());
                    return;
                }
                // Atomic rename .part → final path. If a previous run left a
                // stale final file behind (rare — we only get here on
                // success), QFile::rename refuses to overwrite, so unlink
                // first.
                QFile::remove(outPath);
                if (!outFile->rename(outPath)) {
                    const QString err = outFile->errorString();
                    QFile::remove(outFile->fileName());
                    delete outFile;
                    onFailure(tr("Cannot finalize %1: %2").arg(outPath, err));
                    return;
                }
                delete outFile;
                kLogger.info() << "Downloaded" << videoId << "→" << outPath
                               << "via Piped";
                finalizeDownload(videoId, outPath);
            });
}

// =============================================================================
// yt-dlp (desktop fallback only)
// =============================================================================

void YouTubeService::runYtDlp(const QStringList& args,
        int timeoutMs,
        const std::function<void(const QByteArray&)>& onSuccess,
        const std::function<void(const QString&)>& onFailure) {
    if (m_ytDlpPath.isEmpty()) {
        onFailure(tr("yt-dlp not available"));
        return;
    }

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    // Watchdog timer parented to `proc`; the connect target is also `proc`,
    // so when proc is destroyed both the timer and its lambda are torn down
    // before any post-cleanup fire is possible.
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
    // the errorOccurred handler synthesizes a failure for that one case.
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
                    return; // `finished` will run cleanup
                }
                watchdog->stop();
                proc->deleteLater();
                onFailure(tr("Failed to launch yt-dlp"));
            });

    watchdog->start();
    proc->start(m_ytDlpPath, args);
}

void YouTubeService::searchViaYtDlp(const QString& query, int cap) {
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
                const QJsonObject root =
                        QJsonDocument::fromJson(stdoutBytes).object();
                const QJsonArray entries =
                        root.value(QStringLiteral("entries")).toArray();
                QList<YouTubeVideoInfo> results;
                results.reserve(entries.size());
                for (const QJsonValue& v : entries) {
                    if (results.size() >= cap) {
                        break;
                    }
                    const QJsonObject entry = v.toObject();
                    YouTubeVideoInfo info;
                    info.id = entry.value(QStringLiteral("id")).toString();
                    info.title = entry.value(QStringLiteral("title")).toString();
                    info.uploader =
                            entry.value(QStringLiteral("channel")).toString();
                    if (info.uploader.isEmpty()) {
                        info.uploader = entry.value(QStringLiteral("uploader"))
                                                .toString();
                    }
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

void YouTubeService::downloadViaYtDlp(const QString& videoId, const QString& cacheDir) {
    const QString outTemplate =
            QDir(cacheDir).filePath(QStringLiteral("%(id)s.%(ext)s"));
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
                // Last non-empty stdout line is the final filepath.
                QString outPath;
                const QList<QByteArray> lines = stdoutBytes.split('\n');
                for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
                    const QString line = QString::fromLocal8Bit(*it).trimmed();
                    if (!line.isEmpty() && QFileInfo::exists(line)) {
                        outPath = line;
                        break;
                    }
                }
                if (outPath.isEmpty()) {
                    // Fallback: scan the cache dir for any file with our id.
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
                kLogger.info() << "Downloaded" << videoId << "→" << outPath
                               << "via yt-dlp";
                finalizeDownload(videoId, outPath);
            },
            [this, videoId](const QString& err) {
                kLogger.warning() << "yt-dlp download failed:" << err;
                Q_EMIT downloadFailed(videoId, err);
            });
}

// =============================================================================
// Shared post-download chain: SponsorBlock fetch → in-place cut → emit
// =============================================================================

void YouTubeService::finalizeDownload(const QString& videoId, const QString& outPath) {
    // Fetch SponsorBlock segments, physically cut them out of the file (so
    // duration / BPM / waveform all reflect the music-only length), THEN
    // tell consumers the file is ready. We deliberately do this ourselves
    // rather than via yt-dlp's --sponsorblock-remove because the latter
    // requires a re-encode pass through ffmpeg and can drop quality.
    fetchSponsorSegmentsInternal(videoId,
            [this, videoId, outPath](const QList<SponsorSegment>& segments) {
                if (!segments.isEmpty()) {
                    const bool cut = cutAudioRanges(outPath, segments);
                    if (!cut) {
                        // Cutting failed (e.g. mid-file in the middle of a
                        // packet boundary on opus). Write the sidecar so
                        // SponsorBlockController can fall back to skip-at-
                        // playback during deck use.
                        QFile sidecar(outPath +
                                QStringLiteral(".sponsor.json"));
                        if (sidecar.open(QIODevice::WriteOnly |
                                    QIODevice::Truncate)) {
                            QJsonArray arr;
                            for (const auto& s : segments) {
                                QJsonObject o;
                                o.insert(QStringLiteral("start"), s.start);
                                o.insert(QStringLiteral("end"), s.end);
                                o.insert(QStringLiteral("category"),
                                        s.category);
                                arr.append(o);
                            }
                            sidecar.write(QJsonDocument(arr).toJson(
                                    QJsonDocument::Compact));
                        }
                    }
                }
                Q_EMIT downloadFinished(videoId, outPath);
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
    // non-music sections within music videos. We download audio-only
    // streams from Piped/yt-dlp, so YouTube's pre/mid/post-roll ads are
    // already absent — SponsorBlock only needs to handle the in-content
    // creator-inserted breaks.
    const QUrl url(
            QStringLiteral("https://sponsor.ajay.app/api/skipSegments?videoID=%1"
                           "&categories=[\"sponsor\",\"selfpromo\",\"interaction\","
                           "\"intro\",\"outro\",\"preview\",\"music_offtopic\"]")
                    .arg(videoId));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mixxx/SponsorBlock");
    QNetworkReply* reply = m_pNam->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        QList<SponsorSegment> segments;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonArray array =
                    QJsonDocument::fromJson(reply->readAll()).array();
            for (const QJsonValue& value : array) {
                const QJsonObject obj = value.toObject();
                const QJsonArray seg =
                        obj.value(QStringLiteral("segment")).toArray();
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
