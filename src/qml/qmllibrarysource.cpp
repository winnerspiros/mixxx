#include "qml/qmllibrarysource.h"

#include <qalgorithms.h>
#include <qlist.h>
#include <qqmlengine.h>

#include <QAbstractListModel>
#include <QVariant>
#include <QtDebug>
#include <memory>

#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/mixxxlibraryfeature.h"
#include "library/trackset/playlistfeature.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/browse/browsefeature.h"
#ifdef NETWORKAUTH
#include "library/spotify/spotifyfeature.h"
#include "library/youtube/youtubefeature.h"
#endif
#include "library/treeitemmodel.h"
#include "moc_qmllibrarysource.cpp"
#include "qmllibraryproxy.h"
#include "qmlconfigproxy.h"

AllTrackLibraryFeature::AllTrackLibraryFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QString()),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pLibraryTableModel(pLibrary->trackTableModel()) {
    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
}

void AllTrackLibraryFeature::activate() {
    emit showTrackModel(m_pLibraryTableModel);
}

namespace mixxx {
namespace qml {

QmlLibrarySource::QmlLibrarySource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QObject(parent),
          m_columns(columns) {
}

void QmlLibrarySource::slotShowTrackModel(QAbstractItemModel* pModel) {
    emit requestTrackModel(std::make_shared<QmlLibraryTrackListModel>(columns(), pModel));
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
