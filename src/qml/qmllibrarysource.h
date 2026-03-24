#ifndef QMLLIBRARYSOURCE_H
#define QMLLIBRARYSOURCE_H

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QVariant>
#include <memory>

#include "library/libraryfeature.h"
#include "library/sidebarmodel.h"
#include "library/treeitemmodel.h"
#include "util/parented_ptr.h"

class LibraryTableModel;

class AllTrackLibraryFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    AllTrackLibraryFeature(Library* pLibrary,
            UserSettingsPointer pConfig);
    ~AllTrackLibraryFeature() override;

    QVariant title() override {
        return tr("All...");
    }
    TreeItemModel* sidebarModel() const override {
        return m_pSidebarModel;
    }

    bool hasTrackTable() override {
        return true;
    }

    LibraryTableModel* trackTableModel() const {
        return m_pLibraryTableModel;
    }

    void searchAndActivate(const QString& query);

  public slots:
    void activate() override;

  private:
    LibraryTableModel* m_pLibraryTableModel;

    parented_ptr<TreeItemModel> m_pSidebarModel;
};

namespace mixxx {
namespace qml {

class QmlLibraryTrackListColumn;
class QmlLibraryTrackListModel;

class QmlLibrarySource : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label MEMBER m_label)
    Q_PROPERTY(QString icon MEMBER m_icon)
    Q_PROPERTY(QQmlListProperty<mixxx::qml::QmlLibraryTrackListColumn> columns READ columnsQml)
    Q_CLASSINFO("DefaultProperty", "columns")
    QML_NAMED_ELEMENT(LibrarySource)
    QML_UNCREATABLE("Only accessible via its specialization")
  public:
    explicit QmlLibrarySource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    QQmlListProperty<QmlLibraryTrackListColumn> columnsQml() {
        return {this, &m_columns};
    }

    const QList<QmlLibraryTrackListColumn*>& columns() const {
        return m_columns;
    }
    virtual LibraryFeature* internal() = 0;

  public slots:
    void slotShowTrackModel(QAbstractItemModel* pModel);

  signals:
    void requestTrackModel(std::shared_ptr<mixxx::qml::QmlLibraryTrackListModel> pModel);

  protected:
    QString m_label;
    QString m_icon;
    QList<QmlLibraryTrackListColumn*> m_columns;
};

class QmlLibraryAllTrackSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryAllTrackSource)
  public:
    explicit QmlLibraryAllTrackSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }

  private:
    std::unique_ptr<AllTrackLibraryFeature> m_pLibraryFeature;
};

class QmlLibraryTracksSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryTracksSource)
  public:
    using QmlLibrarySource::QmlLibrarySource;
    LibraryFeature* internal() override;
};

class QmlLibraryPlaylistsSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryPlaylistsSource)
  public:
    using QmlLibrarySource::QmlLibrarySource;
    LibraryFeature* internal() override;
};

class QmlLibraryCratesSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryCratesSource)
  public:
    using QmlLibrarySource::QmlLibrarySource;
    LibraryFeature* internal() override;
};

class QmlLibraryBrowseSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryBrowseSource)
  public:
    using QmlLibrarySource::QmlLibrarySource;
    LibraryFeature* internal() override;
};

class QmlLibrarySpotifySource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibrarySpotifySource)
  public:
    using QmlLibrarySource::QmlLibrarySource;
    LibraryFeature* internal() override;
};

class QmlLibraryYouTubeSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryYouTubeSource)
  public:
    using QmlLibrarySource::QmlLibrarySource;
    LibraryFeature* internal() override;
};

} // namespace qml
} // namespace mixxx

#endif // QMLLIBRARYSOURCE_H
