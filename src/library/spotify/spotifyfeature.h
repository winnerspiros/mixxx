#pragma once

#ifdef NETWORKAUTH
#include <QtNetworkAuth>
#endif

#include "library/baseexternallibraryfeature.h"
#include "util/parented_ptr.h"

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
    parented_ptr<TreeItemModel> m_pSidebarModel;
#ifdef NETWORKAUTH
    QOAuth2AuthorizationCodeFlow m_oauth2;
#endif
};
