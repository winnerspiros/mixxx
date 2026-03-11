#include "library/spotify/spotifyfeature.h"

#include "library/library.h"
#include "library/treeitem.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("SpotifyFeature");
}

SpotifyFeature::SpotifyFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, "spotify"),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
#ifdef NETWORKAUTH
    m_oauth2.setAuthorizationUrl(QUrl("https://accounts.spotify.com/authorize"));
    m_oauth2.setAccessTokenUrl(QUrl("https://accounts.spotify.com/api/token"));
    m_oauth2.setScope("user-library-read streaming");

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, &QDesktopServices::openUrl);
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, &SpotifyFeature::slotAuthGranted);
#endif
}

void SpotifyFeature::activate() {
#ifdef NETWORKAUTH
    if (m_oauth2.status() != QAbstractOAuth::Status::Granted) {
        m_oauth2.grant();
    }
#endif
}

void SpotifyFeature::slotAuthGranted() {
    kLogger.info() << "Spotify Authentication Granted";
}

void SpotifyFeature::searchAndActivate(const QString& query) {
    kLogger.info() << "Searching Spotify for:" << query;
    // Integration with Spotify Search API goes here
}

TreeItemModel* SpotifyFeature::sidebarModel() const {
    return m_pSidebarModel;
}

#include "moc_spotifyfeature.cpp"
