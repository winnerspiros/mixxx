#pragma once

#include "library/baseexternallibraryfeature.h"
#include "util/parented_ptr.h"

class YouTubeFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~YouTubeFeature() override = default;

    QVariant title() override {
        return tr("YouTube");
    }
    void activate() override;
    TreeItemModel* sidebarModel() const override;

    void searchAndActivate(const QString& query);

  private:
    parented_ptr<TreeItemModel> m_pSidebarModel;
};
