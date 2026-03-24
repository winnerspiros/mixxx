#include "qml/qmllibrarysource.h"

#include <QAbstractListModel>
#include <QVariant>
#include <QtDebug>
#include <memory>

#include "library/browse/browsefeature.h"
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/mixxxlibraryfeature.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/playlistfeature.h"

#ifdef NETWORKAUTH
#include "library/spotify/spotifyfeature.h"
#include "library/youtube/youtubefeature.h"
#endif

#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "qml_owned_ptr.h"

AllTrackLibraryFeature::AllTrackLibraryFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QString()),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pLibraryTableModel(pLibrary->trackTableModel()) {
    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
}

AllTrackLibraryFeature::~AllTrackLibraryFeature() = default;

void AllTrackLibraryFeature::activate() {
    Q_EMIT showTrackModel(m_pLibraryTableModel);
}

namespace mixxx {
namespace qml {

QmlLibrarySource::QmlLibrarySource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QObject(parent),
          m_columns(columns) {
}

void QmlLibrarySource::slotShowTrackModel(QAbstractItemModel* pModel) {
    Q_EMIT requestTrackModel(make_qml_owned<QmlLibraryTrackListModel>(columns(), pModel).get());
}

QmlLibraryAllTrackSource::QmlLibraryAllTrackSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibrarySource(parent, columns),
          m_pLibraryFeature(std::make_unique<AllTrackLibraryFeature>(
                  QmlLibraryProxy::get(), QmlConfigProxy::get())) {
    connect(m_pLibraryFeature.get(),
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibrarySource::slotShowTrackModel);
}

LibraryFeature* QmlLibraryTracksSource::internal() {
    auto* pLibrary = QmlLibraryProxy::get();
    return pLibrary ? pLibrary->mixxxLibraryFeature() : nullptr;
}

LibraryFeature* QmlLibraryPlaylistsSource::internal() {
    auto* pLibrary = QmlLibraryProxy::get();
    return pLibrary ? pLibrary->playlistFeature() : nullptr;
}

LibraryFeature* QmlLibraryCratesSource::internal() {
    auto* pLibrary = QmlLibraryProxy::get();
    return pLibrary ? pLibrary->crateFeature() : nullptr;
}

LibraryFeature* QmlLibraryBrowseSource::internal() {
    auto* pLibrary = QmlLibraryProxy::get();
    return pLibrary ? pLibrary->browseFeature() : nullptr;
}

LibraryFeature* QmlLibrarySpotifySource::internal() {
    auto* pLibrary = QmlLibraryProxy::get();
#ifdef NETWORKAUTH
    return pLibrary ? pLibrary->spotifyFeature() : nullptr;
#else
    return nullptr;
#endif
}

LibraryFeature* QmlLibraryYouTubeSource::internal() {
    auto* pLibrary = QmlLibraryProxy::get();
#ifdef NETWORKAUTH
    return pLibrary ? pLibrary->youtubeFeature() : nullptr;
#else
    return nullptr;
#endif
}

} // namespace qml
} // namespace mixxx

#include \"moc_qmllibrarysource.cpp\"
