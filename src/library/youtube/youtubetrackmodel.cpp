#include "library/youtube/youtubetrackmodel.h"

#include <QSharedPointer>
#include <QString>
#include <QUrl>

#include "library/baseexternaltrackmodel.h"
#include "library/basetrackcache.h"
#include "library/dao/trackschema.h"
#include "library/trackcollectionmanager.h"
#include "moc_youtubetrackmodel.cpp"
#include "track/track.h"

const QString YouTubeTrackModel::kPlaceholderScheme =
        QStringLiteral("youtube://");

YouTubeTrackModel::YouTubeTrackModel(QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        QSharedPointer<BaseTrackCache> trackSource)
        : BaseExternalTrackModel(parent,
                  pTrackCollectionManager,
                  "mixxx.db.model.youtube",
                  "youtube_library",
                  trackSource) {
}

TrackPointer YouTubeTrackModel::getTrack(const QModelIndex& index) const {
    // Use the BaseSqlTableModel raw-location accessor so we see exactly
    // what's stored in the youtube_library row, without the
    // QDir::fromNativeSeparators() massaging that getTrackLocation()
    // applies (which would corrupt the "youtube://" scheme).
    const QString rawLocation = getFieldString(
            index, ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION);
    if (rawLocation.startsWith(kPlaceholderScheme)) {
        // Return null — callers that only need metadata (e.g. context-menu
        // builders) must not trigger unintended downloads. Actual load
        // actions go through getTrackUrl() and the loadTrackLocationToPlayer
        // signal path which carries the target deck group.
        return TrackPointer();
    }
    return BaseExternalTrackModel::getTrack(index);
}

QUrl YouTubeTrackModel::getTrackUrl(const QModelIndex& index) const {
    const QString rawLocation = getFieldString(
            index, ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION);
    if (rawLocation.startsWith(kPlaceholderScheme)) {
        QUrl url;
        url.setScheme(QStringLiteral("youtube"));
        url.setPath(rawLocation.mid(kPlaceholderScheme.size()));
        return url;
    }
    return BaseExternalTrackModel::getTrackUrl(index);
}

TrackId YouTubeTrackModel::getTrackId(const QModelIndex& index) const {
    const QString rawLocation = getFieldString(
            index, ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION);
    if (rawLocation.startsWith(kPlaceholderScheme)) {
        return TrackId();
    }
    TrackPointer pTrack = BaseExternalTrackModel::getTrack(index);
    return pTrack ? pTrack->getId() : TrackId();
}

void YouTubeTrackModel::search(const QString& searchText) {
    // Forward filter to the SQL backing — narrows already-loaded rows so
    // there's instant feedback even while the network call is in flight.
    BaseExternalTrackModel::search(searchText);
    // Then ask the feature to fire a fresh YouTube search with the same
    // query. Empty queries are common (the search box clears when the user
    // backspaces everything) — skip the network call in that case.
    const QString trimmed = searchText.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    emit searchRequested(trimmed);
}

QString YouTubeTrackModel::resolveLocation(const QString& nativeLocation) const {
    // Placeholders must NEVER be passed through as a real path — that would
    // make BaseSqlTableModel::getTrackLocation return "youtube://VIDEOID"
    // and downstream code would try to fopen() it. Return an empty string
    // so any "do I have a real file for this row?" caller sees "no". The
    // placeholder path through getTrack() above handles activation.
    if (nativeLocation.startsWith(kPlaceholderScheme)) {
        return QString();
    }
    return BaseExternalTrackModel::resolveLocation(nativeLocation);
}
