#include "library/youtube/youtubefeature.h"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "mixer/playermanager.h"
#include "track/track.h"
#include "track/trackref.h"
#include "util/file.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("YouTubeFeature");

constexpr int kSearchResultsMax = 25;

// We tag the TreeItem `data` payload so activateChild() can tell apart
// "search result the user wants to load" from "already-downloaded track".
const QString kSearchPrefix = QStringLiteral("yt-search:");
const QString kCachedPrefix = QStringLiteral("yt-cached:");
} // namespace

YouTubeFeature::YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, "youtube"),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_service(this) {
    // Allow the user to override the yt-dlp binary path from prefs (useful on
    // Android where we ship the binary inside the APK and resolve a different
    // absolute path at runtime).
    const QString configured = pConfig->getValueString(
            ConfigKey(QStringLiteral("[YouTube]"), QStringLiteral("yt_dlp_path")));
    if (!configured.isEmpty()) {
        m_service.setYtDlpPath(configured);
    }
    // Optional destructive sponsor removal at download time. Off by default;
    // see YouTubeService::setRemoveSponsorsAtDownload for the trade-off.
    m_service.setRemoveSponsorsAtDownload(
            pConfig->getValue(
                    ConfigKey(QStringLiteral("[YouTube]"),
                            QStringLiteral("sponsorblock_remove_at_download")),
                    false));

    connect(&m_service, &mixxx::YouTubeService::searchResultsReady,
            this, &YouTubeFeature::onSearchResultsReady);
    connect(&m_service, &mixxx::YouTubeService::searchFailed,
            this, &YouTubeFeature::onSearchFailed);
    connect(&m_service, &mixxx::YouTubeService::downloadFinished,
            this, &YouTubeFeature::onDownloadFinished);
    connect(&m_service, &mixxx::YouTubeService::downloadFailed,
            this, &YouTubeFeature::onDownloadFailed);

    rebuildSidebar();
}

QString YouTubeFeature::cacheDir() const {
    QString base = m_pConfig->getSettingsPath();
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    const QString dir = QDir(base).filePath(QStringLiteral("youtube_cache"));
    QDir().mkpath(dir);
    return dir;
}

void YouTubeFeature::activate() {
    kLogger.debug() << "YouTube feature activated";
    Q_EMIT switchToView(QStringLiteral("YOUTUBE_HOME"));
    Q_EMIT disableSearch();
    Q_EMIT enableCoverArtDisplay(false);
}

void YouTubeFeature::activateChild(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    const auto* pItem = static_cast<TreeItem*>(index.internalPointer());
    if (!pItem) {
        return;
    }
    const QString payload = pItem->getData().toString();
    if (payload.startsWith(kSearchPrefix)) {
        // User clicked a search result: download (or use cache) and load into
        // the first available deck.
        const QString videoId = payload.mid(kSearchPrefix.size());
        requestDownload(videoId);
    } else if (payload.startsWith(kCachedPrefix)) {
        // User clicked an already-downloaded track: load it now.
        const QString localPath = payload.mid(kCachedPrefix.size());
        TrackRef ref = TrackRef::fromFilePath(localPath);
        TrackPointer pTrack = m_pLibrary->trackCollectionManager()->getOrAddTrack(ref);
        if (pTrack) {
            Q_EMIT loadTrack(pTrack);
        }
    }
}

TreeItemModel* YouTubeFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void YouTubeFeature::searchAndActivate(const QString& query) {
    kLogger.info() << "Searching YouTube for:" << query;
    m_lastQuery = query;
    m_lastResults.clear();
    rebuildSidebar();
    m_service.searchVideos(query, kSearchResultsMax);
}

void YouTubeFeature::onSearchResultsReady(
        const QString& query, const QList<mixxx::YouTubeVideoInfo>& results) {
    if (query != m_lastQuery) {
        return; // a newer search has superseded this one
    }
    m_lastResults = results;
    rebuildSidebar();
}

void YouTubeFeature::onSearchFailed(const QString& query, const QString& error) {
    if (query != m_lastQuery) {
        return;
    }
    kLogger.warning() << "YouTube search failed:" << error;
}

void YouTubeFeature::requestDownload(const QString& videoId) {
    // If we already have a cached file for this id, skip the download and load
    // it straight away.
    const QDir dir(cacheDir());
    const QStringList existing =
            dir.entryList({videoId + QStringLiteral(".*")},
                    QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& f : existing) {
        if (f.endsWith(QStringLiteral(".info.json"))) {
            continue;
        }
        onDownloadFinished(videoId, dir.filePath(f));
        return;
    }
    kLogger.info() << "Downloading YouTube video" << videoId;
    m_service.downloadVideo(videoId, cacheDir());
}

void YouTubeFeature::onDownloadFinished(
        const QString& videoId, const QString& localPath) {
    if (!QFileInfo::exists(localPath)) {
        kLogger.warning() << "Downloaded file disappeared:" << localPath;
        return;
    }
    // Pre-fetch SponsorBlock segments and cache them next to the audio so the
    // SponsorBlockController can pick them up when this track is loaded onto a
    // deck — without depending on network at play time.
    m_service.fetchSponsorSegments(videoId);

    // Look up a friendly label from m_lastResults if the video came from the
    // current search, otherwise just fall back to the videoId.
    QString label = videoId;
    for (const auto& info : std::as_const(m_lastResults)) {
        if (info.id == videoId) {
            label = info.title.isEmpty() ? videoId : info.title;
            break;
        }
    }
    m_downloadedTracks.insert(videoId, label);
    rebuildSidebar();

    // Add to the track collection and load onto the first available deck.
    TrackRef ref = TrackRef::fromFilePath(localPath);
    TrackPointer pTrack = m_pLibrary->trackCollectionManager()->getOrAddTrack(ref);
    if (pTrack) {
        Q_EMIT loadTrack(pTrack);
    } else {
        kLogger.warning() << "Could not add downloaded track to library:"
                          << localPath;
    }
}

void YouTubeFeature::onDownloadFailed(const QString& videoId, const QString& error) {
    kLogger.warning() << "YouTube download failed for" << videoId << ":" << error;
}

void YouTubeFeature::appendTrackIdsFromRightClickIndex(
        QList<TrackId>* /*trackIds*/, QString* /*pPlaylist*/) {
    // YouTube tracks are loaded on demand and we do not track collection ids
    // until they have been imported via getOrAddTrack(). Right-click context
    // actions therefore have nothing to append here.
}

void YouTubeFeature::rebuildSidebar() {
    auto pRoot = TreeItem::newRoot(this);

    if (!m_lastQuery.isEmpty()) {
        TreeItem* pSearchNode = pRoot->appendChild(
                tr("Search: %1").arg(m_lastQuery));
        for (const auto& info : std::as_const(m_lastResults)) {
            const QString durationText = info.durationSec > 0
                    ? QString::fromLatin1("%1:%2")
                              .arg(info.durationSec / 60)
                              .arg(info.durationSec % 60, 2, 10, QLatin1Char('0'))
                    : QString();
            QString label = info.title;
            if (!info.uploader.isEmpty()) {
                label += QStringLiteral(" — ") + info.uploader;
            }
            if (!durationText.isEmpty()) {
                label += QStringLiteral(" [") + durationText + QStringLiteral("]");
            }
            pSearchNode->appendChild(label, kSearchPrefix + info.id);
        }
    }

    if (!m_downloadedTracks.isEmpty()) {
        TreeItem* pCachedNode = pRoot->appendChild(tr("Downloaded"));
        const QDir dir(cacheDir());
        for (auto it = m_downloadedTracks.cbegin();
                it != m_downloadedTracks.cend();
                ++it) {
            const QStringList files =
                    dir.entryList({it.key() + QStringLiteral(".*")},
                            QDir::Files | QDir::NoDotAndDotDot);
            QString localPath;
            for (const QString& f : files) {
                if (f.endsWith(QStringLiteral(".info.json"))) {
                    continue;
                }
                localPath = dir.filePath(f);
                break;
            }
            if (localPath.isEmpty()) {
                continue;
            }
            pCachedNode->appendChild(it.value(), kCachedPrefix + localPath);
        }
    }

    m_pSidebarModel->setRootItem(std::move(pRoot));
}

#include "moc_youtubefeature.cpp"
