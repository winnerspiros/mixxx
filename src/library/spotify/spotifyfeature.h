#pragma once

#include <QDesktopServices>

#include "library/baseexternallibraryfeature.h"
#include "util/parented_ptr.h"

#ifdef NETWORKAUTH
#include <QtNetworkAuth/QOAuth2AuthorizationCodeFlow>
#endif

class SpotifyFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    SpotifyFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~SpotifyFeature() override = default;

    QVariant title() override {
        return tr("Spotify");
    }
    void activate() override;
    TreeItemModel* sidebarModel() const override;

    void searchAndActivate(const QString& query);

  private slots:
    void slotAuthGranted();

  private:
#ifdef NETWORKAUTH
    QOAuth2AuthorizationCodeFlow m_oauth2;
#endif
    parented_ptr<TreeItemModel> m_pSidebarModel;
};
