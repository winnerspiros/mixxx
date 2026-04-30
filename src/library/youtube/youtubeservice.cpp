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
#include <QUrl>
#include <utility>

#include "library/youtube/youtubeaudiocutter.h"
#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("YouTubeService");

// InnerTube is YouTube's internal JSON-RPC-ish API used by every official
// client. The "ANDROID" client has the nicest property for us: its `player`
// responses ship `streamingData.adaptiveFormats[].url` *unsigned*, so we can
// download them with a plain GET. (The WEB client wraps URLs in an obfuscated
// signature that needs JS execution to decode.) The "key" below is the public
// API key shipped in YouTube's own Android app — it identifies the client, it
// is not a secret.
const QByteArray kInnerTubeKey = "AIzaSyA8eiZmM1FaDVjRy-df2KTyQ_vz_yYM39w";
const QString kInnerTubeBase = QStringLiteral(
        "https://www.youtube.com/youtubei/v1/");
const QString kInnerTubeContext = QStringLiteral(R"({
    "client": {
        "clientName": "ANDROID",
        "clientVersion": "19.09.37",
        "androidSdkVersion": 30,
        "hl": "en",
        "gl": "US"
    }
})");

QByteArray makeInnerTubeBody(const QJsonObject& extra) {
    QJsonObject root = extra;
    root.insert(QStringLiteral("context"),
            QJsonDocument::fromJson(kInnerTubeContext.toUtf8()).object());
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString extFromMime(const QString& mime) {
    if (mime.contains(QStringLiteral("webm"))) {
        return QStringLiteral("webm");
    }
    if (mime.contains(QStringLiteral("mp4")) ||
            mime.contains(QStringLiteral("m4a"))) {
        return QStringLiteral("m4a");
    }
    return QStringLiteral("audio");
}
} // namespace

YouTubeService::YouTubeService(QObject* parent)
        : QObject(parent),
          m_pNam(new QNetworkAccessManager(this)) {
}

void YouTubeService::innerTubePost(const QString& endpoint,
        const QByteArray& body,
        const std::function<void(const QByteArray&)>& cb,
        const std::function<void(const QString&)>& errCb) {
    QUrl url(kInnerTubeBase + endpoint);
    url.setQuery(QStringLiteral("key=") + QString::fromLatin1(kInnerTubeKey));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent",
            "com.google.android.youtube/19.09.37 (Linux; U; Android 11)");
    QNetworkReply* reply = m_pNam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [reply, cb, errCb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            errCb(reply->errorString());
            return;
        }
        cb(reply->readAll());
    });
}

void YouTubeService::searchVideos(const QString& query, int cap) {
    if (query.trimmed().isEmpty()) {
        Q_EMIT searchResultsReady(query, {});
        return;
    }
    QJsonObject body;
    body.insert(QStringLiteral("query"), query);
    const QByteArray req = makeInnerTubeBody(body);
    innerTubePost(QStringLiteral("search"), req, [this, query, cap](const QByteArray& data) {
                // Walk InnerTube's deeply-nested response to find videoRenderer
                // entries. Schema is stable enough to traverse by key name.
                const QJsonDocument doc = QJsonDocument::fromJson(data);
                QList<YouTubeVideoInfo> results;
                std::function<void(const QJsonValue&)> walk =
                        [&](const QJsonValue& v) {
                            if (results.size() >= cap) {
                                return;
                            }
                            if (v.isObject()) {
                                const QJsonObject obj = v.toObject();
                                if (obj.contains(QStringLiteral("videoId")) &&
                                        obj.contains(QStringLiteral("title"))) {
                                    YouTubeVideoInfo info;
                                    info.id = obj.value(QStringLiteral("videoId"))
                                                      .toString();
                                    // title is wrapped {"runs":[{"text":...}]} or
                                    // {"simpleText":...}
                                    const QJsonValue title =
                                            obj.value(QStringLiteral("title"));
                                    if (title.isObject()) {
                                        const QJsonObject t = title.toObject();
                                        if (t.contains(QStringLiteral("simpleText"))) {
                                            info.title = t.value(QStringLiteral("simpleText"))
                                                                 .toString();
                                        } else {
                                            const QJsonArray runs =
                                                    t.value(QStringLiteral("runs"))
                                                            .toArray();
                                            for (const auto& r : runs) {
                                                info.title +=
                                                        r.toObject()
                                                                .value(QStringLiteral("text"))
                                                                .toString();
                                            }
                                        }
                                    }
                                    // lengthSeconds is a string in the response.
                                    info.durationSec =
                                            obj.value(QStringLiteral("lengthSeconds"))
                                                    .toString()
                                                    .toInt();
                                    // longBylineText.runs[0].text → uploader
                                    const QJsonArray byline =
                                            obj.value(QStringLiteral("longBylineText"))
                                                    .toObject()
                                                    .value(QStringLiteral("runs"))
                                                    .toArray();
                                    if (!byline.isEmpty()) {
                                        info.uploader =
                                                byline.first()
                                                        .toObject()
                                                        .value(QStringLiteral("text"))
                                                        .toString();
                                    }
                                    if (!info.id.isEmpty() && !info.title.isEmpty()) {
                                        results.append(info);
                                    }
                                    return;
                                }
                                for (auto it = obj.constBegin();
                                        it != obj.constEnd();
                                        ++it) {
                                    walk(it.value());
                                }
                            } else if (v.isArray()) {
                                const QJsonArray arr = v.toArray();
                                for (const auto& el : arr) {
                                    walk(el);
                                }
                            }
                        };
                walk(doc.object());
                kLogger.info() << "InnerTube returned" << results.size()
                               << "results for" << query;
                Q_EMIT searchResultsReady(query, results); }, [this, query](const QString& err) {
                kLogger.warning() << "InnerTube search failed:" << err;
                Q_EMIT searchFailed(query, err); });
}

void YouTubeService::downloadVideo(const QString& videoId, const QString& cacheDir) {
    QDir().mkpath(cacheDir);
    QJsonObject body;
    body.insert(QStringLiteral("videoId"), videoId);
    const QByteArray req = makeInnerTubeBody(body);
    innerTubePost(QStringLiteral("player"), req, [this, videoId, cacheDir](const QByteArray& data) {
                const QJsonObject root = QJsonDocument::fromJson(data).object();
                const QJsonObject streamingData =
                        root.value(QStringLiteral("streamingData")).toObject();
                const QJsonArray adaptive =
                        streamingData.value(QStringLiteral("adaptiveFormats")).toArray();
                // Pick highest-bitrate audio-only stream.
                QString bestUrl;
                QString bestMime;
                int bestBitrate = -1;
                for (const auto& v : adaptive) {
                    const QJsonObject f = v.toObject();
                    const QString mime = f.value(QStringLiteral("mimeType")).toString();
                    if (!mime.startsWith(QStringLiteral("audio/"))) {
                        continue;
                    }
                    const int br = f.value(QStringLiteral("bitrate")).toInt();
                    if (br <= bestBitrate) {
                        continue;
                    }
                    const QString url = f.value(QStringLiteral("url")).toString();
                    if (url.isEmpty()) {
                        // ANDROID client should never give us signatureCipher,
                        // but if YouTube ever changes that we want to fail loudly
                        // rather than ship corrupt files.
                        continue;
                    }
                    bestUrl = url;
                    bestMime = mime;
                    bestBitrate = br;
                }
                if (bestUrl.isEmpty()) {
                    Q_EMIT downloadFailed(videoId,
                            tr("No playable audio stream found for %1").arg(videoId));
                    return;
                }
                const QString outPath = QDir(cacheDir).filePath(
                        videoId + QStringLiteral(".") + extFromMime(bestMime));
                QNetworkReply* reply = m_pNam->get(QNetworkRequest(QUrl(bestUrl)));
                connect(reply, &QNetworkReply::finished, this,
                        [this, reply, videoId, outPath]() {
                            reply->deleteLater();
                            if (reply->error() != QNetworkReply::NoError) {
                                Q_EMIT downloadFailed(videoId, reply->errorString());
                                return;
                            }
                            QFile out(outPath);
                            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                                Q_EMIT downloadFailed(videoId,
                                        tr("Cannot write %1").arg(outPath));
                                return;
                            }
                            out.write(reply->readAll());
                            out.close();
                            kLogger.info() << "Downloaded" << videoId << "→" << outPath;

                            // Chain: fetch SponsorBlock segments, physically
                            // cut them out of the file (so duration / BPM /
                            // waveform all reflect the music-only length),
                            // THEN tell consumers the file is ready.
                            fetchSponsorSegmentsInternal(videoId,
                                    [this, videoId, outPath](
                                            const QList<SponsorSegment>&
                                                    segments) {
                                        if (!segments.isEmpty()) {
                                            const bool cut = cutAudioRanges(
                                                    outPath, segments);
                                            if (!cut) {
                                                // Cutting failed — write the
                                                // sidecar so SponsorBlockController
                                                // can fall back to skip-at-playback.
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
                                                    sidecar.write(
                                                            QJsonDocument(arr).toJson(
                                                                    QJsonDocument::
                                                                            Compact));
                                                }
                                            }
                                        }
                                        Q_EMIT downloadFinished(
                                                videoId, outPath);
                                    });
                        }); }, [this, videoId](const QString& err) {
                kLogger.warning() << "InnerTube player failed:" << err;
                Q_EMIT downloadFailed(videoId, err); });
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
    // download from InnerTube, so there is nothing to block at this layer.
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
