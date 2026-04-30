#pragma once

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QSharedPointer>

#include "library/baseexternallibraryfeature.h"
#include "library/youtube/youtubeservice.h"
#include "util/parented_ptr.h"

class BaseTrackCache;
class KeyboardEventFilter;
class QNetworkAccessManager;
class QSqlDatabase;
class TreeItem;
class WLibrary;
class WLibraryTextBrowser;
class YouTubeTrackModel;

class YouTubeFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~YouTubeFeature() override = default;

    QVariant title() override {
        return tr("YouTube");
    }
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    TreeItemModel* sidebarModel() const override;
    void bindLibraryWidget(WLibrary* pLibraryWidget,
            KeyboardEventFilter* pKeyboard) override;

    void searchAndActivate(const QString& query);

    /// Like searchAndActivate, but in addition to populating the sidebar,
    /// auto-loads the first (top) YouTube result onto the next free deck once
    /// the search returns. Used by the Spotify→YouTube bridge so that
    /// clicking a Spotify track gives the user audio without an extra step.
    /// `displayLabel` is shown in logs/UI to make the cross-source mapping
    /// transparent.
    void searchAndAutoLoadFirst(const QString& query, const QString& displayLabel = QString());

    /// Absolute path to the per-user yt-dlp cache directory. Created on demand.
    QString cacheDir() const;

    /// Resolve the ISO 3166-1 alpha-2 region used for the YouTube "trending"
    /// feed. Resolution order:
    ///   1. `[YouTube]/trending_region` user override (if non-empty)
    ///   2. `[YouTube]/detected_region` previously auto-detected via geo-IP
    ///   3. `QLocale::system().territory()` / `country()` (Qt 6 / Qt 5)
    ///   4. The literal "GR" (Greece) — explicit project default when nothing
    ///      else is known. This used to be "US", which surprised users (see
    ///      "Trending in United States" bug report).
    /// Always returns a non-empty 2-letter uppercase code.
    QString resolvedTrendingRegion() const;

  signals:
    /// Emitted whenever a fresh geo-IP lookup updates the cached region.
    /// `YouTubeFeature::activate()` listens to this so the trending feed
    /// auto-refreshes once the user's actual country is known, even if the
    /// pane was opened before the network call finished.
    void detectedRegionChanged(const QString& region);

  protected:
    void appendTrackIdsFromRightClickIndex(
            QList<TrackId>* trackIds, QString* pPlaylist) override;

  private slots:
    void onSearchResultsReady(
            const QString& query, const QList<mixxx::YouTubeVideoInfo>& results);
    void onSearchFailed(const QString& query, const QString& error);
    void onDownloadFinished(const QString& videoId, const QString& localPath);
    void onDownloadFailed(const QString& videoId, const QString& error);
    /// Dispatch clicks on links rendered in the home pane HTML.
    /// `ytplay:VIDEOID`     → download (or load from cache) and load on a deck.
    /// `ytcached:LOCALPATH` → load the already-downloaded file on a deck.
    void onHomeAnchorClicked(const QUrl& url);

  private:
    /// Rebuild the sidebar tree from the current search-result and
    /// downloaded-track caches.
    void rebuildSidebar();
    /// Rebuild the HTML for the main YOUTUBE_HOME pane (the right-hand area
    /// the user sees when YouTube is selected). Mirrors the sidebar listing
    /// so the user has feedback that a search returned results, even before
    /// they unfold the tree node.
    void rebuildHomeHtml();
    /// Trigger a download (or short-circuit if already cached) for `videoId`.
    /// The downloaded track will be auto-loaded onto the next free deck.
    void requestDownload(const QString& videoId);
    /// Like requestDownload but does NOT load onto a deck — used for
    /// background pre-fetch / repair of missing AutoDJ-queued tracks.
    void requestPrefetch(const QString& videoId);
    /// If `pTrack` was downloaded by us and is no longer loaded on any deck,
    /// delete its cached audio file and purge it from the library DB so the
    /// disk doesn't grow unbounded. Tracks referenced by any playlist or
    /// crate (incl. the AutoDJ queue, which is itself a hidden playlist) are
    /// preserved.
    void maybeReleaseCachedTrack(const TrackPointer& pTrack);
    /// If `pTrack` is a YouTube-cache track whose file is missing, kick off
    /// a background re-download so the next play attempt succeeds. No-op for
    /// non-YouTube tracks or already-present files.
    void ensureDownloaded(const TrackPointer& pTrack);
    /// Walk the AutoDJ queue at startup and ensure every YouTube-cache track
    /// in it is present on disk — re-downloading any that have been swept.
    void prefetchAutoDjQueue();
    /// Kick off an async geo-IP lookup against api.country.is (no API key,
    /// no client setup). On success, persist the ISO 3166-1 alpha-2 country
    /// code under `[YouTube]/detected_region` and emit
    /// `detectedRegionChanged`. On any failure (offline, blocked, parse
    /// error) the call is a silent no-op — `resolvedTrendingRegion()` will
    /// keep returning whatever it had before (locale → "GR").
    void detectRegionAsync();

    QNetworkAccessManager* m_pNam = nullptr;

    /// Persistent track table backing the right-hand pane. See
    /// YouTubeTrackModel for the placeholder/downloaded row model.
    QSharedPointer<BaseTrackCache> m_pTrackCache;
    YouTubeTrackModel* m_pTrackModel = nullptr;
    /// Replace all rows in `youtube_library` with the supplied YouTube
    /// videos, then refresh the model so the view picks up the new rows.
    /// `videos` may include both downloaded (file present in cacheDir())
    /// and not-yet-downloaded entries; we resolve which is which row by
    /// row when building the INSERT.
    void replaceTrackTable(const QList<mixxx::YouTubeVideoInfo>& videos);
    /// Append a single downloaded entry (or update its row to point at the
    /// real file path) so the "Downloaded" column reflects the new file
    /// without a full table rebuild.
    void upsertDownloadedRow(const QString& videoId,
            const QString& localPath,
            const QString& title,
            const QString& uploader,
            int durationSec);

    parented_ptr<TreeItemModel> m_pSidebarModel;
    QPointer<WLibraryTextBrowser> m_pHomeView;
    mixxx::YouTubeService m_service;
    QString m_lastQuery;
    QList<mixxx::YouTubeVideoInfo> m_lastResults;
    // videoId -> human-readable label (used for the "Downloaded" branch).
    QHash<QString, QString> m_downloadedTracks;
    // videoIds whose download was triggered by a user click and should be
    // auto-loaded onto a deck once finished. Background prefetch downloads
    // are NOT in this set, so they don't yank the deck.
    QSet<QString> m_videoIdsToAutoLoad;
    /// When set, the next batch of search results that matches `m_lastQuery`
    /// will trigger an auto-download+load of the top result. Cleared once
    /// consumed so subsequent user-driven searches don't accidentally autoload.
    bool m_autoLoadNextResult = false;
    QString m_autoLoadDisplayLabel;
    /// Last error message reported by the underlying YouTubeService for the
    /// current `m_lastQuery`. Cleared when a new search is started or when
    /// results arrive successfully. When non-empty, the home pane shows the
    /// error in place of the perpetual "Searching…" placeholder.
    QString m_lastSearchError;
};
