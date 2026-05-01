#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QUrl>

#include "library/baseexternaltrackmodel.h"
#include "library/basetrackcache.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
struct YouTubeVideoInfo;
}

/// Track-table model for the YouTube library feature. Backed by the
/// persistent `youtube_library` SQL table (see schema.xml revision 41) so
/// that the standard `WTrackTableView` UI Just Works — sortable columns,
/// drag-to-deck, right-click → Add to Auto DJ, BPM/Key after analysis, etc.
///
/// Two kinds of rows live in `youtube_library`:
///
///   1. **Downloaded** rows whose `location` is a real path on disk under
///      the YouTube cache dir. These behave exactly like any other external
///      library track: `getTrack()` calls `getOrAddTrack(location)` and the
///      deck loads the file directly.
///
///   2. **Placeholder** rows whose `location` is the sentinel
///      `youtube://VIDEOID`. `getTrack()` recognizes the sentinel, emits
///      `requestDownloadAndLoad(videoId)` so the feature kicks off a
///      download (with auto-load on completion via the existing
///      `m_videoIdsToAutoLoad`/`onDownloadFinished` chain), and returns a
///      null `TrackPointer`. The deck won't load anything immediately —
///      `LibraryFeature::loadTrack` will fire once the download lands.
///      This is the "double-click does the right thing eventually with no
///      extra clicks" UX the user asked for.
class YouTubeTrackModel : public BaseExternalTrackModel {
    Q_OBJECT
  public:
    /// Sentinel location-column scheme for not-yet-downloaded rows. Stored
    /// as the row's `location` because `BaseSqlTableModel` enforces a
    /// UNIQUE on that column — using the videoId in the URL guarantees
    /// uniqueness without an extra index.
    static const QString kPlaceholderScheme;

    YouTubeTrackModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            QSharedPointer<BaseTrackCache> trackSource);

    TrackPointer getTrack(const QModelIndex& index) const override;
    QUrl getTrackUrl(const QModelIndex& index) const override;
    TrackId getTrackId(const QModelIndex& index) const override;
    /// Override of BaseSqlTableModel::search — in addition to filtering the
    /// existing rows (parent behaviour), dispatches a fresh
    /// `YouTubeService::searchVideos(searchText)` request via the feature
    /// so the table fills with live results as the user types in the
    /// per-view search box.
    void search(const QString& searchText) override;

  signals:
    /// Emitted from `getTrack()` when the user activates a placeholder
    /// row. The feature's slot turns this into a download request with
    /// auto-load-on-completion semantics.
    void requestDownloadAndLoad(const QString& videoId);
    /// Emitted from `search()` to ask the feature to dispatch a fresh
    /// `YouTubeService::searchVideos(query)`. The feature owns the
    /// service and the network/auth state so we keep that in one place.
    void searchRequested(const QString& query);

  protected:
    QString resolveLocation(const QString& nativeLocation) const override;
};
