#include "library/spotify/spotifyfeature.h"

#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <functional>
#include <memory>

#include "library/library.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "library/youtube/youtubefeature.h"
#include "preferences/configobject.h"
#include "util/logger.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarytextbrowser.h"

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

const QString kPlaylistPrefix = QStringLiteral("spotify-playlist:");
const QString kTrackPrefix = QStringLiteral("spotify-track:");
const QString kInfoPrefix = QStringLiteral("spotify-info:");
// Separator used between track title and artist when constructing display
// labels. Centralized so the YouTube-bridge parsing in activateChild stays
// in sync with all label producers (refreshLibrary / searchAndActivate /
// playlist-track expansion).
const QString kLabelSep = QStringLiteral(" — ");
} // namespace

SpotifyFeature::SpotifyFeature(Library* pLibrary,
        UserSettingsPointer pConfig,
        YouTubeFeature* pYouTubeFeature)
        : BaseExternalLibraryFeature(pLibrary, pConfig, "spotify"),
          m_pNam(new QNetworkAccessManager(this)),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pYouTubeFeature(pYouTubeFeature) {
#ifdef NETWORKAUTH
    // Reply handler runs a tiny HTTP server on localhost that Spotify will
    // redirect back to with the authorization code.
    // Port 0 lets the OS pick a free port; we tell the user which port is in
    // use via the auth URL Spotify opens in the browser.
    constexpr quint16 kRedirectPort = 0;
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

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, &QDesktopServices::openUrl);
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, &SpotifyFeature::slotAuthGranted);

    loadPersistedTokens();
#endif
    rebuildSidebar();
}

void SpotifyFeature::activate() {
    kLogger.debug() << "Spotify feature activated";
    Q_EMIT switchToView(QStringLiteral("SPOTIFY_HOME"));
    // Intentionally do NOT emit disableSearch(): Library forwards search box
    // input to SpotifyFeature::searchAndActivate, so leaving the box enabled
    // is the only way the user can drive a Spotify search.
    Q_EMIT enableCoverArtDisplay(false);
    rebuildHomeHtml();

#ifdef NETWORKAUTH
    QString clientId = m_pConfig->getValueString(kCfgClientId);
    if (clientId.isEmpty()) {
        clientId = promptForClientId();
        if (clientId.isEmpty()) {
            // User cancelled. Leave the pane in its "set up to continue" state.
            return;
        }
        m_oauth2.setClientIdentifier(clientId);
    }
    if (m_oauth2.status() != QAbstractOAuth::Status::Granted) {
        // QOAuth2AuthorizationCodeFlow::authorizeWithBrowser opens the URL
        // via QDesktopServices::openUrl. On Android the system handles deep
        // links: if the official Spotify app is installed it intercepts the
        // accounts.spotify.com/authorize URL and shows the in-app consent
        // screen; otherwise the user falls back to the browser. Either way
        // they end up redirected to the local reply handler.
        m_oauth2.grant();
    } else {
        refreshLibrary();
    }
#endif
}

void SpotifyFeature::bindLibraryWidget(WLibrary* pLibraryWidget,
        KeyboardEventFilter* /*pKeyboard*/) {
    // Without registering a widget for SPOTIFY_HOME the right-hand library
    // pane renders as a blank grey rectangle — the user has no idea whether
    // they're authenticated or what to do next.
    auto pBrowser = make_parented<WLibraryTextBrowser>(pLibraryWidget);
    pBrowser->setOpenLinks(false);
    m_pHomeView = pBrowser.get();
    pLibraryWidget->registerView(QStringLiteral("SPOTIFY_HOME"), pBrowser);
    rebuildHomeHtml();
}

QString SpotifyFeature::promptForClientId() {
#ifdef NETWORKAUTH
    bool ok = false;
    const QString existing = m_pConfig->getValueString(kCfgClientId);
    const QString entered = QInputDialog::getText(nullptr,
            tr("Spotify setup"),
            tr("To enable Spotify, register a free developer application "
               "at https://developer.spotify.com/dashboard, set its redirect "
               "URI to http://localhost (any port), copy the Client ID and "
               "paste it below.\n\nClient ID:"),
            QLineEdit::Normal,
            existing,
            &ok);
    if (!ok) {
        return QString();
    }
    const QString trimmed = entered.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    m_pConfig->set(kCfgClientId, ConfigValue(trimmed));
    return trimmed;
#else
    return QString();
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
                        const QJsonObject t =
                                item.toObject()
                                        .value(QStringLiteral("track"))
                                        .toObject();
                        if (t.isEmpty()) {
                            continue;
                        }
                        Item it;
                        it.label = t.value(QStringLiteral("name")).toString();
                        const QJsonArray artists = t.value(QStringLiteral("artists")).toArray();
                        if (!artists.isEmpty()) {
                            it.label += kLabelSep +
                                    artists.first()
                                            .toObject()
                                            .value(QStringLiteral("name"))
                                            .toString();
                        }
                        it.uri = t.value(QStringLiteral("uri")).toString();
                        tracks.append(it);
                    }
                    m_searchResults = tracks;
                    m_lastQuery = tr("Playlist %1").arg(playlistId);
                    rebuildSidebar();
                    rebuildHomeHtml();
                });
    } else if (payload.startsWith(kTrackPrefix)) {
        // Spotify's Web API does not return audio (only 30s previews) and
        // the official "Play with Spotify" partner program was discontinued
        // in July 2020. The pragmatic approach used by every modern tool
        // that "plays Spotify" (spotdl, savify, etc.) is to use Spotify
        // strictly as a metadata source and fetch the actual audio from
        // YouTube. We do the same here. The pane's home HTML makes this
        // explicit so the user is not misled about where the audio comes
        // from.
        const QString uri = payload.mid(kTrackPrefix.size());
        if (!m_pYouTubeFeature) {
            QMessageBox::information(nullptr,
                    tr("Spotify playback unavailable"),
                    tr("YouTube backend not available, cannot resolve "
                       "Spotify track.\n\nTrack URI: %1")
                            .arg(uri));
            return;
        }
        // The sidebar label is "Title — Artist" by construction (see
        // refreshLibrary / searchAndActivate / activateChild for playlists).
        // Strip the unicode em-dash for a cleaner YouTube query.
        const QString label = pItem->getLabel();
        QString query = label;
        query.replace(kLabelSep, QStringLiteral(" "));
        m_pYouTubeFeature->searchAndAutoLoadFirst(query, label);
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
                        it.label += kLabelSep +
                                artists.first().toObject().value(QStringLiteral("name")).toString();
                    }
                    it.uri = t.value(QStringLiteral("uri")).toString();
                    results.append(it);
                }
                m_searchResults = results;
                rebuildSidebar();
                rebuildHomeHtml();
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
    rebuildHomeHtml();
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
            // We use a manual one-shot connection (rather than Qt 6.0+'s
            // Qt::SingleShotConnection) so this works on Qt 5 builds too.
            auto pConn = std::make_shared<QMetaObject::Connection>();
            *pConn = connect(&m_oauth2,
                    &QOAuth2AuthorizationCodeFlow::granted,
                    this,
                    [this, endpoint, cb, pConn]() {
                        QObject::disconnect(*pConn);
                        persistTokens();
                        apiGet(endpoint, cb);
                    });
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
                        it.label += kLabelSep +
                                artists.first().toObject().value(QStringLiteral("name")).toString();
                    }
                    it.uri = t.value(QStringLiteral("uri")).toString();
                    items.append(it);
                }
                m_savedTracks = items;
                rebuildSidebar();
                rebuildHomeHtml();
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
                rebuildHomeHtml();
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
#ifdef NETWORKAUTH
        pRoot->appendChild(
                tr("Sign in to Spotify (click \"Spotify\" above)"),
                QString(kInfoPrefix + QStringLiteral("auth")));
#else
        pRoot->appendChild(
                tr("Sign-in unavailable on this build (no Qt NetworkAuth)"),
                QString(kInfoPrefix + QStringLiteral("auth")));
#endif
    }

    // Honest disclosure: Spotify's API does not return audio, so clicking a
    // track resolves it to YouTube and plays from there. Surface this in the
    // sidebar so the user understands what's happening.
    pRoot->appendChild(
            tr("ℹ Track audio is sourced from YouTube"),
            QString(kInfoPrefix + QStringLiteral("source")));

    if (!m_playlists.isEmpty()) {
        TreeItem* pPlaylists = pRoot->appendChild(tr("Playlists"));
        for (const auto& it : std::as_const(m_playlists)) {
            pPlaylists->appendChild(it.label, QString(kPlaylistPrefix + it.uri));
        }
    }
    if (!m_savedTracks.isEmpty()) {
        TreeItem* pSaved = pRoot->appendChild(tr("Liked Songs"));
        for (const auto& it : std::as_const(m_savedTracks)) {
            pSaved->appendChild(it.label, QString(kTrackPrefix + it.uri));
        }
    }
    if (!m_searchResults.isEmpty()) {
        TreeItem* pSearch = pRoot->appendChild(
                m_lastQuery.isEmpty() ? tr("Search") : tr("Results: %1").arg(m_lastQuery));
        for (const auto& it : std::as_const(m_searchResults)) {
            pSearch->appendChild(it.label, QString(kTrackPrefix + it.uri));
        }
    }

    m_pSidebarModel->setRootItem(std::move(pRoot));
}

void SpotifyFeature::rebuildHomeHtml() {
    if (!m_pHomeView) {
        return;
    }
    QString html;
    html += QStringLiteral("<h2>") + tr("Spotify") + QStringLiteral("</h2>");

#ifndef NETWORKAUTH
    html += QStringLiteral("<p><b>") +
            tr("Spotify sign-in is not available on this build (Qt was built "
               "without the NetworkAuth module). YouTube search still works "
               "from the sidebar — type a song or artist in the library "
               "search box.") +
            QStringLiteral("</b></p>");
#else
    const bool needsAuth = m_oauth2.status() != QAbstractOAuth::Status::Granted;
    const QString clientId = m_pConfig->getValueString(kCfgClientId);
    if (clientId.isEmpty()) {
        html += QStringLiteral("<p>") +
                tr("To enable Spotify, click the Spotify entry in the "
                   "sidebar — you will be prompted for a Spotify Client ID. "
                   "(Register a free app at "
                   "https://developer.spotify.com/dashboard, set its "
                   "redirect URI to http://localhost, and copy the Client "
                   "ID.)") +
                QStringLiteral("</p>");
    } else if (needsAuth) {
        html += QStringLiteral("<p>") +
                tr("Click the Spotify entry in the sidebar to sign in. The "
                   "Spotify app will open for authorization on Android, or "
                   "your default browser on desktop.") +
                QStringLiteral("</p>");
    } else {
        html += QStringLiteral("<p>") +
                tr("Signed in. Type a song or artist in the search box at "
                   "the top of the library to search Spotify, or browse "
                   "your Liked Songs and playlists in the sidebar.") +
                QStringLiteral("</p>");
    }
#endif

    html += QStringLiteral("<p><i>") +
            tr("Note: Spotify's API does not return audio. When you click a "
               "Spotify track, DJ Sugar searches YouTube for the same "
               "title+artist and downloads the audio from there. The "
               "downloaded file is auto-cleaned when the track is ejected "
               "from all decks and not in any playlist or crate.") +
            QStringLiteral("</i></p>");

    if (!m_searchResults.isEmpty()) {
        html += QStringLiteral("<h3>") +
                (m_lastQuery.isEmpty()
                                ? tr("Results")
                                : tr("Results: %1").arg(m_lastQuery.toHtmlEscaped())) +
                QStringLiteral("</h3><ul>");
        for (const auto& it : std::as_const(m_searchResults)) {
            html += QStringLiteral("<li>") + it.label.toHtmlEscaped() +
                    QStringLiteral("</li>");
        }
        html += QStringLiteral("</ul>");
    }
    if (!m_savedTracks.isEmpty()) {
        html += QStringLiteral("<h3>") + tr("Liked Songs") +
                QStringLiteral("</h3><ul>");
        for (const auto& it : std::as_const(m_savedTracks)) {
            html += QStringLiteral("<li>") + it.label.toHtmlEscaped() +
                    QStringLiteral("</li>");
        }
        html += QStringLiteral("</ul>");
    }
    if (!m_playlists.isEmpty()) {
        html += QStringLiteral("<h3>") + tr("Playlists") +
                QStringLiteral("</h3><ul>");
        for (const auto& it : std::as_const(m_playlists)) {
            html += QStringLiteral("<li>") + it.label.toHtmlEscaped() +
                    QStringLiteral("</li>");
        }
        html += QStringLiteral("</ul>");
    }

    m_pHomeView->setHtml(html);
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
