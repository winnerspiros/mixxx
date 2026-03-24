#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QVariant>
#include <memory>

#include "library/libraryfeature.h"
#include "qmllibrarytracklistcolumn.h"

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCountQml NOTIFY rowCountChanged)
    Q_PROPERTY(QQmlListProperty<::mixxx::qml::QmlLibraryTrackListColumn> columns READ columnsQml CONSTANT)
    QML_ANONYMOUS
  public:
    QmlLibraryTrackListModel(
            const QList<QmlLibraryTrackListColumn*>& columns,
            QAbstractItemModel* pModel,
            QObject* parent = nullptr);
    ~QmlLibraryTrackListModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCountQml() const {
        return rowCount();
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QQmlListProperty<::mixxx::qml::QmlLibraryTrackListColumn> columnsQml() {
        return {this, &m_columns};
    }

    Q_INVOKABLE QVariant get(int row) const;

  signals:
    void rowCountChanged();

  private:
    QList<QmlLibraryTrackListColumn*> m_columns;
    QAbstractItemModel* m_pModel;
    QHash<int, int> m_roleToColumn;
};

} // namespace qml
} // namespace mixxx

Q_DECLARE_METATYPE(std::shared_ptr<mixxx::qml::QmlLibraryTrackListModel>)
