#include "library/spotify/spotifyfeature.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include "library/library.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "preferences/configobject.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("SpotifyFeature");

const ConfigKey kCfgClientId(
        QStringLiteral("[Spotify]"), QStringLiteral("client_id"));
const ConfigKey kCfgAccessToken(
        QStringLiteral("[Spotify]"), QStringLiteral("access_token"));
const ConfigKey kCfgRefreshToken(
        QStringLiteral("[Spotify]"), QStringLiteral("refresh_token"));
const ConfigKey kCfgExpiresAt(
        QStringLiteral("[Spotify]"), QStringLiteral("expires_at"));

// Spotify's OAuth Authorization Code with PKCE flow runs the redirect against
// a localhost loopback. Port 0 lets the OS pick a free port; we tell the user
// which port is in use via the auth URL Spotify opens in the browser.
constexpr quint16 kRedirectPort = 0;

const QString kSearchPrefix = QStringLiteral("spotify-search:");
const QString kPlaylistPrefix = QStringLiteral("spotify-playlist:");
const QString kTrackPrefix = QStringLiteral("spotify-track:");
const QString kInfoPrefix = QStringLiteral("spotify-info:");
} // namespace

SpotifyFeature::SpotifyFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, "spotify"),
          m_pNam(new QNetworkAccessManager(this)),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
#ifdef NETWORKAUTH
    // Reply handler runs a tiny HTTP server on localhost that Spotify will
    // redirect back to with the authorization code.
    m_pReplyHandler = new QOAuthHttpServerReplyHandler(kRedirectPort, this);

    m_oauth2.setAuthorizationUrl(QUrl(QStringLiteral("https://accounts.spotify.com/authorize")));
    m_oauth2.setAccessTokenUrl(QUrl(QStringLiteral("https://accounts.spotify.com/api/token")));
    // Browse-only scopes — playlist-modify is intentionally NOT requested.
    m_oauth2.setScope(QStringLiteral(
            "user-library-read playlist-read-private playlist-read-collaborative streaming"));
    m_oauth2.setReplyHandler(m_pReplyHandler);

    // Client id has to be supplied by the user — we cannot ship a public secret
    // for a fork. Without it Spotify will not start the auth flow.
    const QString clientId = m_pConfig->getValueString(kCfgClientId);
    if (!clientId.isEmpty()) {
        m_oauth2.setClientIdentifier(clientId);
    }

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser,
            &QDesktopServices::openUrl);
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::granted,
            this, &SpotifyFeature::slotAuthGranted);

    loadPersistedTokens();
#endif
    rebuildSidebar();
}

void SpotifyFeature::activate() {
    kLogger.debug() << "Spotify feature activated";
    Q_EMIT switchToView(QStringLiteral("SPOTIFY_HOME"));
    Q_EMIT disableSearch();
    Q_EMIT enableCoverArtDisplay(false);

#ifdef NETWORKAUTH
    if (m_pConfig->getValueString(kCfgClientId).isEmpty()) {
        QMessageBox::information(nullptr,
                tr("Spotify setup required"),
                tr("To use Spotify in DJ Sugar, register a free Spotify "
                   "developer application at "
                   "https://developer.spotify.com/dashboard, set its redirect "
                   "URI to http://localhost (any port), and store the Client "
                   "ID under [Spotify]/client_id in your DJ Sugar config "
                   "file."));
        return;
    }
    if (m_oauth2.status() != QAbstractOAuth::Status::Granted) {
        m_oauth2.grant();
    } else {
        refreshLibrary();
    }
#endif
}

void SpotifyFeature::activateChild(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    const auto* pItem = static_cast<TreeItem*>(index.internalPointer());
    if (!pItem) {
        return;
    }
    const QString payload = pItem->getData().toString();
    if (payload.startsWith(kPlaylistPrefix)) {
        const QString playlistId = payload.mid(kPlaylistPrefix.size());
        apiGet(QStringLiteral("playlists/") + playlistId + QStringLiteral("/tracks?limit=100"),
                [this, playlistId](const QJsonDocument& doc) {
                    QList<Item> tracks;
                    for (const auto& item : doc.object().value(QStringLiteral("items")).toArray()) {
                        const QJsonObject t = item.toObject().value(QStringLiteral("track")).toObject();
                        if (t.isEmpty()) {
                            continue;
                        }
                        Item it;
                        it.label = t.value(QStringLiteral("name")).toString();
                        const QJsonArray artists = t.value(QStringLiteral("artists")).toArray();
                        if (!artists.isEmpty()) {
                            it.label += QStringLiteral(" — ") +
                                    artists.first().toObject().value(QStringLiteral("name")).toString();
                        }
                        it.uri = t.value(QStringLiteral("uri")).toString();
                        tracks.append(it);
                    }
                    m_searchResults = tracks;
                    m_lastQuery = tr("Playlist %1").arg(playlistId);
                    rebuildSidebar();
                });
    } else if (payload.startsWith(kTrackPrefix)) {
        // Spotify track URIs cannot currently be played without a librespot
        // backend. Surface this honestly rather than silently failing.
        QMessageBox::information(nullptr,
                tr("Spotify playback not yet available"),
                tr("Playback of Spotify tracks requires a librespot bridge "
                   "(Spotify Premium account required) which is not yet "
                   "bundled with DJ Sugar.\n\n"
                   "Track URI: %1")
                        .arg(payload.mid(kTrackPrefix.size())));
    } else if (payload.startsWith(kInfoPrefix)) {
        // Pure info node, do nothing.
    }
}

TreeItemModel* SpotifyFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void SpotifyFeature::searchAndActivate(const QString& query) {
    if (query.trimmed().isEmpty()) {
        return;
    }
    m_lastQuery = query;
#ifdef NETWORKAUTH
    if (m_oauth2.status() != QAbstractOAuth::Status::Granted) {
        kLogger.info() << "Spotify search requested but not authenticated yet";
        return;
    }
#endif
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("type"), QStringLiteral("track"));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("25"));
    apiGet(QStringLiteral("search?") + q.query(),
            [this](const QJsonDocument& doc) {
                QList<Item> results;
                const QJsonArray items = doc.object()
                                                 .value(QStringLiteral("tracks"))
                                                 .toObject()
                                                 .value(QStringLiteral("items"))
                                                 .toArray();
                for (const auto& v : items) {
                    const QJsonObject t = v.toObject();
                    Item it;
                    it.label = t.value(QStringLiteral("name")).toString();
                    const QJsonArray artists = t.value(QStringLiteral("artists")).toArray();
                    if (!artists.isEmpty()) {
                        it.label += QStringLiteral(" — ") +
                                artists.first().toObject().value(QStringLiteral("name")).toString();
                    }
                    it.uri = t.value(QStringLiteral("uri")).toString();
                    results.append(it);
                }
                m_searchResults = results;
                rebuildSidebar();
            });
}

void SpotifyFeature::appendTrackIdsFromRightClickIndex(
        QList<TrackId>* /*trackIds*/, QString* /*pPlaylist*/) {
    // Spotify items are not in the local TrackCollection, so nothing to do.
}

#ifdef NETWORKAUTH
void SpotifyFeature::slotAuthGranted() {
    kLogger.info() << "Spotify authentication granted";
    persistTokens();
    refreshLibrary();
}
#endif

void SpotifyFeature::apiGet(
        const QString& endpoint,
        std::function<void(const QJsonDocument&)> cb) {
#ifdef NETWORKAUTH
    if (m_oauth2.status() != QAbstractOAuth::Status::Granted) {
        kLogger.warning() << "Spotify API call attempted without auth:" << endpoint;
        return;
    }
    QNetworkRequest req(QUrl(QStringLiteral("https://api.spotify.com/v1/") + endpoint));
    req.setRawHeader("Authorization",
            (QStringLiteral("Bearer ") + m_oauth2.token()).toUtf8());
    QNetworkReply* reply = m_pNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint, cb]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            cb(QJsonDocument::fromJson(reply->readAll()));
            return;
        }
        // 401 means token is stale — try a refresh and replay the request once.
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            kLogger.info() << "Spotify token expired, refreshing";
            connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this,
                    [this, endpoint, cb]() {
                        persistTokens();
                        apiGet(endpoint, cb);
                    },
                    Qt::SingleShotConnection);
            m_oauth2.refreshAccessToken();
            return;
        }
        kLogger.warning() << "Spotify API error" << endpoint
                          << ":" << reply->errorString();
    });
#else
    Q_UNUSED(endpoint);
    Q_UNUSED(cb);
#endif
}

void SpotifyFeature::refreshLibrary() {
    apiGet(QStringLiteral("me/tracks?limit=50"),
            [this](const QJsonDocument& doc) {
                QList<Item> items;
                for (const auto& v : doc.object().value(QStringLiteral("items")).toArray()) {
                    const QJsonObject t = v.toObject().value(QStringLiteral("track")).toObject();
                    if (t.isEmpty()) {
                        continue;
                    }
                    Item it;
                    it.label = t.value(QStringLiteral("name")).toString();
                    const QJsonArray artists = t.value(QStringLiteral("artists")).toArray();
                    if (!artists.isEmpty()) {
                        it.label += QStringLiteral(" — ") +
                                artists.first().toObject().value(QStringLiteral("name")).toString();
                    }
                    it.uri = t.value(QStringLiteral("uri")).toString();
                    items.append(it);
                }
                m_savedTracks = items;
                rebuildSidebar();
            });
    apiGet(QStringLiteral("me/playlists?limit=50"),
            [this](const QJsonDocument& doc) {
                QList<Item> items;
                for (const auto& v : doc.object().value(QStringLiteral("items")).toArray()) {
                    const QJsonObject p = v.toObject();
                    Item it;
                    it.label = p.value(QStringLiteral("name")).toString();
                    it.uri = p.value(QStringLiteral("id")).toString();
                    items.append(it);
                }
                m_playlists = items;
                rebuildSidebar();
            });
}

void SpotifyFeature::rebuildSidebar() {
    auto pRoot = TreeItem::newRoot(this);

#ifdef NETWORKAUTH
    const bool needsAuth = m_oauth2.status() != QAbstractOAuth::Status::Granted;
#else
    constexpr bool needsAuth = true;
#endif

    if (needsAuth) {
        pRoot->appendChild(
                tr("Sign in to Spotify (click \"Spotify\" above)"),
                kInfoPrefix + QStringLiteral("auth"));
    }

    // Honest disclosure: nobody likes silent feature gaps.
    pRoot->appendChild(
            tr("⚠ Playback requires librespot (not yet bundled)"),
            kInfoPrefix + QStringLiteral("librespot"));

    if (!m_playlists.isEmpty()) {
        TreeItem* pPlaylists = pRoot->appendChild(tr("Playlists"));
        for (const auto& it : std::as_const(m_playlists)) {
            pPlaylists->appendChild(it.label, kPlaylistPrefix + it.uri);
        }
    }
    if (!m_savedTracks.isEmpty()) {
        TreeItem* pSaved = pRoot->appendChild(tr("Liked Songs"));
        for (const auto& it : std::as_const(m_savedTracks)) {
            pSaved->appendChild(it.label, kTrackPrefix + it.uri);
        }
    }
    if (!m_searchResults.isEmpty()) {
        TreeItem* pSearch = pRoot->appendChild(
                m_lastQuery.isEmpty() ? tr("Search") : tr("Results: %1").arg(m_lastQuery));
        for (const auto& it : std::as_const(m_searchResults)) {
            pSearch->appendChild(it.label, kTrackPrefix + it.uri);
        }
    }

    m_pSidebarModel->setRootItem(std::move(pRoot));
}

void SpotifyFeature::persistTokens() {
#ifdef NETWORKAUTH
    m_pConfig->set(kCfgAccessToken, ConfigValue(m_oauth2.token()));
    m_pConfig->set(kCfgRefreshToken, ConfigValue(m_oauth2.refreshToken()));
    if (m_oauth2.expirationAt().isValid()) {
        m_pConfig->set(kCfgExpiresAt,
                ConfigValue(QString::number(m_oauth2.expirationAt().toSecsSinceEpoch())));
    }
#endif
}

void SpotifyFeature::loadPersistedTokens() {
#ifdef NETWORKAUTH
    const QString refresh = m_pConfig->getValueString(kCfgRefreshToken);
    if (refresh.isEmpty()) {
        return;
    }
    // Restoring just the refresh token is enough — QOAuth2 will exchange it for
    // a fresh access token on the first authenticated request.
    m_oauth2.setRefreshToken(refresh);
    const QString token = m_pConfig->getValueString(kCfgAccessToken);
    if (!token.isEmpty()) {
        m_oauth2.setToken(token);
    }
#endif
}

#include "moc_spotifyfeature.cpp"
