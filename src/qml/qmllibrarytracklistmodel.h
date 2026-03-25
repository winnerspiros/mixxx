#pragma once

#include <QIdentityProxyModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QUrl>
#include <memory>

#include "library/trackmodel.h"
#include "qml/qmllibrarytracklistcolumn.h"
#include "qml/qmltrackproxy.h"

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel : public QIdentityProxyModel {
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QQmlListProperty<::mixxx::qml::QmlLibraryTrackListColumn> columns READ columns CONSTANT)
    QML_ANONYMOUS
  public:
    enum Role {
        DelegateRole = Qt::UserRole + 1,
        TrackProxyRole,
        FileURLRole,
        CoverArtRole,
    };
    Q_ENUM(Role)

    QmlLibraryTrackListModel(
            const QList<QmlLibraryTrackListColumn*>& librarySource,
            QAbstractItemModel* pModel,
            QObject* pParent = nullptr);
    ~QmlLibraryTrackListModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QQmlListProperty<::mixxx::qml::QmlLibraryTrackListColumn> columns() {
        return {this, &m_columns};
    }

    Q_INVOKABLE QUrl getUrl(int row) const;
    Q_INVOKABLE ::mixxx::qml::QmlTrackProxy* getTrack(int row) const;

    Q_INVOKABLE TrackModel::Capabilities getCapabilities() const;
    Q_INVOKABLE bool hasCapabilities(TrackModel::Capabilities caps) const;

    Q_INVOKABLE void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

  signals:
    void rowCountChanged();

  private:
    QList<QmlLibraryTrackListColumn*> m_columns;
};

} // namespace qml
} // namespace mixxx
