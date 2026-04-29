#pragma once

#include <QHash>

#include "library/baseexternallibraryfeature.h"
#include "library/youtube/youtubeservice.h"
#include "util/parented_ptr.h"

class TreeItem;

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

    void searchAndActivate(const QString& query);

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
    /// Trigger a download (or short-circuit if already cached) for `videoId`.
    void requestDownload(const QString& videoId);

    parented_ptr<TreeItemModel> m_pSidebarModel;
    mixxx::YouTubeService m_service;
    QString m_lastQuery;
    QList<mixxx::YouTubeVideoInfo> m_lastResults;
    // videoId -> human-readable label (used for the "Downloaded" branch).
    QHash<QString, QString> m_downloadedTracks;
};
