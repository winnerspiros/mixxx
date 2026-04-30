#include "library/youtube/youtubefeature.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "library/dao/trackschema.h"
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "track/track.h"
#include "track/trackref.h"
#include "util/file.h"
#include "util/logger.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarytextbrowser.h"

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
    connect(&m_service,
            &mixxx::YouTubeService::searchResultsReady,
            this,
            &YouTubeFeature::onSearchResultsReady);
    connect(&m_service,
            &mixxx::YouTubeService::searchFailed,
            this,
            &YouTubeFeature::onSearchFailed);
    connect(&m_service,
            &mixxx::YouTubeService::downloadFinished,
            this,
            &YouTubeFeature::onDownloadFinished);
    connect(&m_service,
            &mixxx::YouTubeService::downloadFailed,
            this,
            &YouTubeFeature::onDownloadFailed);

    // Auto-cleanup: when a YouTube-cached track is ejected from a deck (i.e.
    // replaced by a new one or unloaded), and no other deck still has it
    // loaded, delete the cached audio + sponsorblock sidecar from disk and
    // purge the database entry. This gives the user the "search → on deck →
    // forget about it" experience without unbounded disk growth. Analysis
    // results (BPM/key/waveform) are already persisted at this point so they
    // are not lost — only the audio bytes are.
    connect(&PlayerInfo::instance(),
            &PlayerInfo::trackChanged,
            this,
            [this](const QString& /*group*/,
                    TrackPointer pNew,
                    TrackPointer pOld) {
                // Pre-download safety net: if AutoDJ (or the user from a stale
                // playlist) is loading a YouTube-cache track whose file no
                // longer exists, kick off a background re-download so the
                // next play attempt or analysis pass succeeds without the
                // user noticing a stall.
                ensureDownloaded(pNew);
                if (pOld) {
                    maybeReleaseCachedTrack(pOld);
                }
            });

    // At startup, pre-fetch every YouTube-cache track that's queued in AutoDJ
    // but no longer present on disk. This is the "I closed the app halfway
    // through a set, restart, hit play" case — without this you'd hear a gap
    // when AutoDJ tried to crossfade into the missing track.
    QTimer::singleShot(0, this, [this]() {
        prefetchAutoDjQueue();
    });

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
    // Intentionally do NOT emit disableSearch(): the main library search box
    // is the only way the user can drive a YouTube search (Library forwards
    // queries to YouTubeFeature::searchAndActivate). Greying it out — which
    // is what BaseExternalLibraryFeature siblings do — would make the pane
    // appear empty with no way to populate it.
    Q_EMIT enableCoverArtDisplay(false);
    rebuildHomeHtml();
    // Prime the pane with a "what's hot" list on first open so the user has
    // something to click before they've typed anything. We piggyback on the
    // existing search path rather than hitting a dedicated trending endpoint
    // (InnerTube's `browse:FEtrending` exists but would double the API
    // surface in YouTubeService for marginal benefit).
    if (m_lastQuery.isEmpty() && m_lastResults.isEmpty()) {
        // Use an untranslated literal as the API query — translating
        // "trending music" would send a localized phrase to YouTube and
        // produce inconsistent (or empty) results for non-English users.
        searchAndActivate(QStringLiteral("trending music"));
    }
}

void YouTubeFeature::bindLibraryWidget(WLibrary* pLibraryWidget,
        KeyboardEventFilter* /*pKeyboard*/) {
    // The YOUTUBE_HOME view backs the right-hand library pane when the user
    // selects "YouTube" in the sidebar. Without this registration the pane
    // would render as an empty grey rectangle (the sidebar tree still
    // populates separately, but the user has no main-area feedback that the
    // search ran).
    auto pBrowser = make_parented<WLibraryTextBrowser>(pLibraryWidget);
    pBrowser->setOpenLinks(false);
    m_pHomeView = pBrowser.get();
    pLibraryWidget->registerView(QStringLiteral("YOUTUBE_HOME"), pBrowser);
    rebuildHomeHtml();
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
    m_lastSearchError.clear();
    m_autoLoadNextResult = false;
    m_autoLoadDisplayLabel.clear();
    rebuildSidebar();
    rebuildHomeHtml();
    m_service.searchVideos(query, kSearchResultsMax);
}

void YouTubeFeature::searchAndAutoLoadFirst(
        const QString& query, const QString& displayLabel) {
    kLogger.info() << "Searching YouTube for auto-load:" << query
                   << "(display:" << displayLabel << ")";
    m_lastQuery = query;
    m_lastResults.clear();
    m_lastSearchError.clear();
    m_autoLoadNextResult = true;
    m_autoLoadDisplayLabel = displayLabel.isEmpty() ? query : displayLabel;
    rebuildSidebar();
    rebuildHomeHtml();
    m_service.searchVideos(query, kSearchResultsMax);
}

void YouTubeFeature::onSearchResultsReady(
        const QString& query, const QList<mixxx::YouTubeVideoInfo>& results) {
    if (query != m_lastQuery) {
        return; // a newer search has superseded this one
    }
    m_lastResults = results;
    m_lastSearchError.clear();
    rebuildSidebar();
    rebuildHomeHtml();
    if (m_autoLoadNextResult && !results.isEmpty()) {
        const QString videoId = results.first().id;
        kLogger.info() << "Auto-loading top YouTube hit" << videoId
                       << "for" << m_autoLoadDisplayLabel;
        requestDownload(videoId);
    }
    // Always clear the auto-load flag once we've handled this batch — empty
    // results, mismatched-query results, and a successful auto-load alike.
    m_autoLoadNextResult = false;
}

void YouTubeFeature::onSearchFailed(const QString& query, const QString& error) {
    if (query != m_lastQuery) {
        return;
    }
    kLogger.warning() << "YouTube search failed:" << error;
    m_lastSearchError = error;
    // A failed search must not keep an auto-load pending — otherwise the next
    // unrelated search would silently inherit the auto-load intent.
    m_autoLoadNextResult = false;
    rebuildHomeHtml();
}

void YouTubeFeature::requestDownload(const QString& videoId) {
    m_videoIdsToAutoLoad.insert(videoId);
    // If we already have a cached file for this id, skip the download and load
    // it straight away.
    const QDir dir(cacheDir());
    const QStringList existing =
            dir.entryList({videoId + QStringLiteral(".*")},
                    QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& f : existing) {
        if (f.endsWith(QStringLiteral(".info.json")) ||
                f.endsWith(QStringLiteral(".sponsor.json"))) {
            continue;
        }
        onDownloadFinished(videoId, dir.filePath(f));
        return;
    }
    kLogger.info() << "Downloading YouTube video" << videoId;
    m_service.downloadVideo(videoId, cacheDir());
}

void YouTubeFeature::requestPrefetch(const QString& videoId) {
    // Background re-download — do NOT register for auto-load. Only kicks off
    // if the file isn't already there.
    const QDir dir(cacheDir());
    const QStringList existing =
            dir.entryList({videoId + QStringLiteral(".*")},
                    QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& f : existing) {
        if (!f.endsWith(QStringLiteral(".info.json")) &&
                !f.endsWith(QStringLiteral(".sponsor.json"))) {
            return; // already on disk
        }
    }
    kLogger.info() << "Pre-fetching YouTube video" << videoId;
    m_service.downloadVideo(videoId, cacheDir());
}

void YouTubeFeature::onDownloadFinished(
        const QString& videoId, const QString& localPath) {
    if (!QFileInfo::exists(localPath)) {
        kLogger.warning() << "Downloaded file disappeared:" << localPath;
        return;
    }
    // SponsorBlock cuts have already been physically applied to the file by
    // YouTubeService at this point (or, on cut failure, a .sponsor.json
    // sidecar has been written so SponsorBlockController can fall back to
    // skip-at-playback). Either way, the file we hand to the analyzer here
    // already represents the music-only timeline.

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
    rebuildHomeHtml();

    // Always register with the track collection so analysis runs and the DB
    // entry exists. Only emit loadTrack for downloads triggered by a user
    // click — background prefetches must not yank what's currently on a deck.
    TrackRef ref = TrackRef::fromFilePath(localPath);
    TrackPointer pTrack = m_pLibrary->trackCollectionManager()->getOrAddTrack(ref);
    if (!pTrack) {
        kLogger.warning() << "Could not add downloaded track to library:"
                          << localPath;
        return;
    }
    if (m_videoIdsToAutoLoad.remove(videoId)) {
        Q_EMIT loadTrack(pTrack);
    }
}

void YouTubeFeature::onDownloadFailed(const QString& videoId, const QString& error) {
    kLogger.warning() << "YouTube download failed for" << videoId << ":" << error;
}

void YouTubeFeature::maybeReleaseCachedTrack(const TrackPointer& pTrack) {
    if (!pTrack) {
        return;
    }
    const QString location = pTrack->getLocation();
    if (location.isEmpty()) {
        return;
    }
    // Only files we own under our cache dir are eligible for cleanup.
    const QDir cache(cacheDir());
    if (!QFileInfo(location).absoluteFilePath().startsWith(
                cache.absolutePath() + QLatin1Char('/'))) {
        return;
    }
    // Don't delete if any other deck still has it loaded — common when the
    // user practices crossfading the same track between decks.
    const auto loaded = PlayerInfo::instance().getLoadedTracks();
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it) {
        if (it.value() && it.value()->getLocation() == location) {
            return;
        }
    }
    // Don't delete if the track is referenced by *anything* the user might
    // come back to: any playlist (which includes the AutoDJ queue — that's a
    // hidden playlist of type AutoDJ), or any crate. This is what makes
    // pre-queued AutoDJ sets safe — queued YouTube tracks survive eject.
    auto* pTcm = m_pLibrary->trackCollectionManager();
    if (!pTcm) {
        return;
    }
    auto* pInternal = pTcm->internalCollection();
    const TrackId trackId = pTrack->getId();
    if (trackId.isValid()) {
        QSet<int> playlistSet;
        pInternal->getPlaylistDAO().getPlaylistsTrackIsIn(trackId, &playlistSet);
        if (!playlistSet.isEmpty()) {
            return;
        }
        if (pInternal->crates().selectTrackCratesSorted(trackId).next()) {
            return;
        }
    }
    // All checks passed → safe to drop bytes + DB row. Defer the unlink so the
    // engine has a moment to release any open file descriptor on slow Android
    // storage; failure here is non-fatal because the next sweep will retry.
    QString videoId = QFileInfo(location).completeBaseName();
    QTimer::singleShot(2000, this, [this, location, videoId, trackId]() {
        if (QFile::remove(location)) {
            kLogger.info() << "Released cached YouTube track" << videoId;
        }
        // SponsorBlock sidecar lives next to the audio file.
        const QString sidecar = QFileInfo(location).absoluteFilePath() +
                QStringLiteral(".sponsor.json");
        QFile::remove(sidecar);
        if (trackId.isValid()) {
            m_pLibrary->trackCollectionManager()->purgeTracks(
                    {TrackRef::fromFilePath(location, trackId)});
        }
        m_downloadedTracks.remove(videoId);
        rebuildSidebar();
        rebuildHomeHtml();
    });
}

void YouTubeFeature::ensureDownloaded(const TrackPointer& pTrack) {
    if (!pTrack) {
        return;
    }
    const QString location = pTrack->getLocation();
    if (location.isEmpty()) {
        return;
    }
    // Only re-fetch tracks under our cache dir.
    const QDir cache(cacheDir());
    if (!QFileInfo(location).absoluteFilePath().startsWith(
                cache.absolutePath() + QLatin1Char('/'))) {
        return;
    }
    if (QFileInfo::exists(location)) {
        return; // already on disk
    }
    const QString videoId = QFileInfo(location).completeBaseName();
    if (videoId.isEmpty()) {
        return;
    }
    requestPrefetch(videoId);
}

void YouTubeFeature::prefetchAutoDjQueue() {
    auto* pTcm = m_pLibrary->trackCollectionManager();
    if (!pTcm) {
        return;
    }
    auto* pInternal = pTcm->internalCollection();
    const int autoDjId = pInternal->getPlaylistDAO().getPlaylistIdFromName(
            AUTODJ_TABLE);
    if (autoDjId < 0) {
        return;
    }
    const QList<TrackId> ids =
            pInternal->getPlaylistDAO().getTrackIdsInPlaylistOrder(autoDjId);
    for (const TrackId& id : ids) {
        TrackPointer pTrack = pTcm->getTrackById(id);
        ensureDownloaded(pTrack);
    }
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
            pSearchNode->appendChild(label, QString(kSearchPrefix + info.id));
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
                // Skip both yt-dlp's metadata sidecar (.info.json) and our
                // SponsorBlock sidecar (.sponsor.json) — only the audio file
                // is loadable. Without this guard, depending on filesystem
                // sort order we could wire the sidebar entry to a JSON file.
                if (f.endsWith(QStringLiteral(".info.json")) ||
                        f.endsWith(QStringLiteral(".sponsor.json"))) {
                    continue;
                }
                localPath = dir.filePath(f);
                break;
            }
            if (localPath.isEmpty()) {
                continue;
            }
            pCachedNode->appendChild(it.value(), QString(kCachedPrefix + localPath));
        }
    }

    m_pSidebarModel->setRootItem(std::move(pRoot));
}

void YouTubeFeature::rebuildHomeHtml() {
    if (!m_pHomeView) {
        return;
    }
    QString html;
    html += QStringLiteral("<h2>") + tr("YouTube") + QStringLiteral("</h2>");

    html += QStringLiteral("<p>") +
            tr("Type a song, artist or video title in the search box at the "
               "top of the library to search YouTube. Click a result in the "
               "sidebar to download it — finished tracks appear under "
               "<b>Downloaded</b> and in the main library view, ready to drag "
               "onto any deck. Audio is downloaded ad-free and SponsorBlock "
               "automatically trims sponsored intros, self-promo and other "
               "non-music segments out of the file.") +
            QStringLiteral("</p>");

    if (!m_lastQuery.isEmpty()) {
        html += QStringLiteral("<h3>") +
                tr("Results for: %1").arg(m_lastQuery.toHtmlEscaped()) +
                QStringLiteral("</h3>");
        if (!m_lastSearchError.isEmpty()) {
            // Surface the underlying error so the user can act on it (e.g.
            // network failure, yt-dlp out-of-date, region-blocked content).
            html += QStringLiteral("<p><b>") +
                    tr("Search failed") +
                    QStringLiteral(":</b> ") +
                    m_lastSearchError.toHtmlEscaped() +
                    QStringLiteral("</p>");
        } else if (m_lastResults.isEmpty()) {
            html += QStringLiteral("<p><i>") +
                    tr("Searching…") +
                    QStringLiteral("</i></p>");
        } else {
            html += QStringLiteral("<ul>");
            for (const auto& info : std::as_const(m_lastResults)) {
                QString line = info.title.toHtmlEscaped();
                if (!info.uploader.isEmpty()) {
                    line += QStringLiteral(" — ") +
                            info.uploader.toHtmlEscaped();
                }
                if (info.durationSec > 0) {
                    line += QStringLiteral(" [%1:%2]")
                                    .arg(info.durationSec / 60)
                                    .arg(info.durationSec % 60,
                                            2,
                                            10,
                                            QLatin1Char('0'));
                }
                html += QStringLiteral("<li>") + line + QStringLiteral("</li>");
            }
            html += QStringLiteral("</ul>");
        }
    }

    if (!m_downloadedTracks.isEmpty()) {
        html += QStringLiteral("<h3>") + tr("Downloaded") + QStringLiteral("</h3>");
        html += QStringLiteral("<ul>");
        for (auto it = m_downloadedTracks.cbegin();
                it != m_downloadedTracks.cend();
                ++it) {
            html += QStringLiteral("<li>") + it.value().toHtmlEscaped() +
                    QStringLiteral("</li>");
        }
        html += QStringLiteral("</ul>");
    }

    m_pHomeView->setHtml(html);
}

#include "moc_youtubefeature.cpp"
