#pragma once

#include <QHash>
#include <QPointer>
#include <QSet>

#include "library/baseexternallibraryfeature.h"
#include "library/youtube/youtubeservice.h"
#include "util/parented_ptr.h"

class KeyboardEventFilter;
class TreeItem;
class WLibrary;
class WLibraryTextBrowser;

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

  protected:
    void appendTrackIdsFromRightClickIndex(
            QList<TrackId>* trackIds, QString* pPlaylist) override;

  private slots:
    void onSearchResultsReady(
            const QString& query, const QList<mixxx::YouTubeVideoInfo>& results);
    void onSearchFailed(const QString& query, const QString& error);
    void onDownloadFinished(const QString& videoId, const QString& localPath);
    void onDownloadFailed(const QString& videoId, const QString& error);

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
