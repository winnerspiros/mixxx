#include "qml/qmllibrarytracklistmodel.h"

#include <QObject>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QColor>

#include "library/basetracktablemodel.h"
#include "library/columncache.h"
#include "qml/asyncimageprovider.h"
#include "qml/qmllibrarytracklistcolumn.h"
#include "qml/qmltrackproxy.h"
#include "qml_owned_ptr.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/parented_ptr.h"

namespace mixxx {
namespace qml {
namespace {
const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "display"},
        {Qt::DecorationRole, "decoration"},
        {QmlLibraryTrackListModel::DelegateRole, "delegate"},
        {QmlLibraryTrackListModel::TrackProxyRole, "track"},
        {QmlLibraryTrackListModel::FileURLRole, "file_url"},
        {QmlLibraryTrackListModel::CoverArtRole, "cover_art"},
};

QColor colorFromRgbCode(double colorValue) {
    if (colorValue < 0 || colorValue > 0xFFFFFF) {
        return {};
    }

    QRgb rgbValue = static_cast<QRgb>(colorValue) | 0xFF000000;
    return QColor(rgbValue);
}
} // namespace

QmlLibraryTrackListModel::QmlLibraryTrackListModel(
        const QList<QmlLibraryTrackListColumn*>& librarySource,
        QAbstractItemModel* pModel,
        QObject* pParent)
        : QIdentityProxyModel(pParent),
          m_columns() {
    m_columns.reserve(librarySource.size());
    for (const auto* pColumn : librarySource) {
        m_columns.append(make_parented<QmlLibraryTrackListColumn>(this,
                pColumn->label(),
                pColumn->fillSpan(),
                pColumn->columnIdx(),
                pColumn->preferredWidth(),
                pColumn->autoHideWidth(),
                pColumn->delegate(),
                pColumn->role()));
    }

    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel);
    if (pTrackModel) {
        pTrackModel->select();
    }
    setSourceModel(pModel);
}

int QmlLibraryTrackListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return sourceModel() ? sourceModel()->rowCount() : 0;
}

int QmlLibraryTrackListModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_columns.size();
}

QVariant QmlLibraryTrackListModel::data(const QModelIndex& proxyIndex, int role) const {
    if (!proxyIndex.isValid()) {
        return {};
    }

    auto columnIdx = proxyIndex.column();
    if (columnIdx < 0 || columnIdx >= m_columns.size()) {
        return {};
    }

    auto* const pTrackTableModel = qobject_cast<BaseTrackTableModel*>(sourceModel());
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());

    const auto& pColumn = m_columns[columnIdx];

    switch (role) {
    case TrackProxyRole: {
        if (pTrackModel == nullptr) {
            return {};
        }
        auto pTrack = pTrackModel->getTrack(mapToSource(proxyIndex));
        if (!pTrack) {
            return {};
        }
        return QVariant::fromValue(make_qml_owned<QmlTrackProxy>(pTrack).get());
    }
    case Qt::DecorationRole: {
        if (pTrackTableModel == nullptr) {
            return {};
        };
        return colorFromRgbCode(QIdentityProxyModel::data(
                proxyIndex.siblingAtColumn(pTrackTableModel->fieldIndex(
                        ColumnCache::COLUMN_LIBRARYTABLE_COLOR)),
                Qt::DisplayRole)
                        .toDouble());
    }
    case CoverArtRole: {
        QString location;
        if (pTrackTableModel != nullptr) {
            location = QIdentityProxyModel::data(
                    proxyIndex.siblingAtColumn(pTrackTableModel->fieldIndex(
                            ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION)),
                    Qt::DisplayRole)
                               .toString();
        } else if (pTrackModel != nullptr) {
            auto pTrack = pTrackModel->getTrack(mapToSource(proxyIndex));
            if (pTrack) {
                location = pTrack->getCoverInfo().coverLocation;
            }
        }
        if (location.isEmpty()) {
            return {};
        }

        return AsyncImageProvider::trackLocationToCoverArtUrl(location);
    }
    case FileURLRole: {
        if (pTrackModel == nullptr) {
            return {};
        }
        return pTrackModel->getTrackUrl(mapToSource(proxyIndex));
    }
    case DelegateRole:
        return QVariant::fromValue(pColumn->delegate());
    }

    if (pColumn->columnIdx() < 0) {
        return QIdentityProxyModel::data(proxyIndex, role);
    }

    return QIdentityProxyModel::data(
            proxyIndex.siblingAtColumn(pTrackTableModel != nullptr
                            ? pTrackTableModel->fieldIndex(
                                      static_cast<ColumnCache::Column>(
                                              pColumn->columnIdx()))
                            : pColumn->columnIdx()),
            role);
}

QVariant QmlLibraryTrackListModel::headerData(
        int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || section < 0 || section >= m_columns.size()) {
        return {};
    }
    if (role == Qt::DisplayRole) {
        return m_columns[section]->label();
    }
    return {};
}

QHash<int, QByteArray> QmlLibraryTrackListModel::roleNames() const {
    return kRoleNames;
}

QUrl QmlLibraryTrackListModel::getUrl(int row) const {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());
    if (pTrackModel == nullptr) {
        return {};
    }
    return pTrackModel->getTrackUrl(sourceModel()->index(row, 0));
}

QmlTrackProxy* QmlLibraryTrackListModel::getTrack(int row) const {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());
    if (pTrackModel == nullptr) {
        return nullptr;
    }
    auto pTrack = pTrackModel->getTrack(sourceModel()->index(row, 0));
    if (!pTrack) {
        return nullptr;
    }
    return make_qml_owned<QmlTrackProxy>(pTrack).get();
}

TrackModel::Capabilities QmlLibraryTrackListModel::getCapabilities() const {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());
    if (pTrackModel != nullptr) {
        return pTrackModel->getCapabilities();
    }
    return TrackModel::Capability::None;
}

bool QmlLibraryTrackListModel::hasCapabilities(TrackModel::Capabilities caps) const {
    return (getCapabilities() & caps) == caps;
}

void QmlLibraryTrackListModel::sort(int column, Qt::SortOrder order) {
    if (column < 0 || column >= m_columns.size()) {
        return;
    }
    const auto& pColumn = m_columns[column];
    Q_EMIT layoutAboutToBeChanged(QList<QPersistentModelIndex>(),
            QAbstractItemModel::VerticalSortHint);
    if (pColumn->columnIdx() < 0) {
        sourceModel()->sort(column, order);
    } else {
        auto* const pTrackTableModel = qobject_cast<BaseTrackTableModel*>(sourceModel());
        sourceModel()->sort(pTrackTableModel != nullptr
                        ? pTrackTableModel->fieldIndex(
                                  static_cast<ColumnCache::Column>(
                                          pColumn->columnIdx()))
                        : pColumn->columnIdx(),
                order);
    }
    Q_EMIT layoutChanged(QList<QPersistentModelIndex>(), QAbstractItemModel::VerticalSortHint);
}

} // namespace qml
} // namespace mixxx

#include "moc_qmllibrarytracklistmodel.cpp"
