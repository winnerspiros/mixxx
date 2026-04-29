#pragma once

#include <QDesktopServices>
#include <QHash>

#include "library/baseexternallibraryfeature.h"
#include "util/parented_ptr.h"

#ifdef NETWORKAUTH
#include <QtNetworkAuth/QOAuth2AuthorizationCodeFlow>
#include <QtNetworkAuth/QOAuthHttpServerReplyHandler>
#endif

class QNetworkAccessManager;
class QNetworkReply;
class TreeItem;

class SpotifyFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    SpotifyFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~SpotifyFeature() override = default;

    QVariant title() override {
        return tr("Spotify");
    }
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    TreeItemModel* sidebarModel() const override;

    void searchAndActivate(const QString& query);

  protected:
    void appendTrackIdsFromRightClickIndex(
            QList<TrackId>* trackIds, QString* pPlaylist) override;

  private slots:
#ifdef NETWORKAUTH
    void slotAuthGranted();
#endif

  private:
    /// Send a `GET https://api.spotify.com/v1/<endpoint>` with the bearer token,
    /// invoke `cb` with the parsed JSON document on success.
    void apiGet(const QString& endpoint,
            std::function<void(const QJsonDocument&)> cb);
    /// Refresh the user's saved tracks + playlists into the sidebar.
    void refreshLibrary();
    void rebuildSidebar();
    /// Persist the most recently obtained access/refresh tokens so the user is
    /// not re-prompted on every launch.
    void persistTokens();
    void loadPersistedTokens();

#ifdef NETWORKAUTH
    QOAuth2AuthorizationCodeFlow m_oauth2;
    QOAuthHttpServerReplyHandler* m_pReplyHandler = nullptr;
#endif
    QNetworkAccessManager* m_pNam = nullptr;
    parented_ptr<TreeItemModel> m_pSidebarModel;

    struct Item {
        QString label;
        QString uri;
    };
    QList<Item> m_savedTracks;
    QList<Item> m_playlists;
    QList<Item> m_searchResults;
    QString m_lastQuery;
};
