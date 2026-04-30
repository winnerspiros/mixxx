#pragma once

#include <QDesktopServices>
#include <QHash>
#include <QPointer>
#include <functional>

#include "library/baseexternallibraryfeature.h"
#include "util/parented_ptr.h"

#ifdef NETWORKAUTH
#include <QtNetworkAuth/QOAuth2AuthorizationCodeFlow>
#include <QtNetworkAuth/QOAuthHttpServerReplyHandler>
#endif

class KeyboardEventFilter;
class QJsonDocument;
class QNetworkAccessManager;
class TreeItem;
class WLibrary;
class WLibraryTextBrowser;
class YouTubeFeature;

class SpotifyFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    SpotifyFeature(Library* pLibrary,
            UserSettingsPointer pConfig,
            YouTubeFeature* pYouTubeFeature = nullptr);
    ~SpotifyFeature() override = default;

    QVariant title() override {
        return tr("Spotify");
    }
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    TreeItemModel* sidebarModel() const override;
    void bindLibraryWidget(WLibrary* pLibraryWidget,
            KeyboardEventFilter* pKeyboard) override;

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
            const std::function<void(const QJsonDocument&)>& cb);
    /// Refresh the user's saved tracks + playlists into the sidebar.
    void refreshLibrary();
    void rebuildSidebar();
    /// Rebuild the HTML for the right-hand SPOTIFY_HOME pane (mirrors the
    /// sidebar so the user gets feedback in the main area).
    void rebuildHomeHtml();
    /// Prompt the user for a Spotify Client ID via QInputDialog and persist
    /// it under [Spotify]/client_id. Returns the entered (and stored) value
    /// or an empty string if the user cancelled.
    QString promptForClientId();
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
    QPointer<WLibraryTextBrowser> m_pHomeView;
    /// Non-owning. Used to resolve Spotify track clicks to a downloadable
    /// YouTube video — Spotify's API does not return audio.
    YouTubeFeature* m_pYouTubeFeature = nullptr;

    struct Item {
        QString label;
        QString uri;
    };
    QList<Item> m_savedTracks;
    QList<Item> m_playlists;
    QList<Item> m_searchResults;
    QString m_lastQuery;
};
