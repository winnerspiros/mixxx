#include "qml/qmlsuggestionsmodel.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "mixer/playerinfo.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmlplayermanagerproxy.h"
#include "track/track.h"
#include "util/logger.h"

namespace mixxx {
namespace qml {

namespace {
const Logger kLogger("QmlSuggestionsModel");

QSet<int> currentlyLoadedTrackIds() {
    QSet<int> ids;
    const auto loaded = PlayerInfo::instance().getLoadedTracks();
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it) {
        if (it.value() && it.value()->getId().isValid()) {
            ids.insert(it.value()->getId().value());
        }
    }
    return ids;
}
} // namespace

QmlSuggestionsModel::QmlSuggestionsModel(QObject* parent)
        : QAbstractListModel(parent) {
    refresh();
}

int QmlSuggestionsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

QHash<int, QByteArray> QmlSuggestionsModel::roleNames() const {
    return {
            {TrackIdRole, "trackId"},
            {TitleRole, "title"},
            {ArtistRole, "artist"},
            {DurationSecRole, "durationSec"},
            {BpmRole, "bpm"},
            {LocationRole, "location"},
            {SourceRole, "source"},
    };
}

QVariant QmlSuggestionsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case TrackIdRole:
        return r.trackId;
    case TitleRole:
        return r.title;
    case ArtistRole:
        return r.artist;
    case DurationSecRole:
        return r.durationSec;
    case BpmRole:
        return r.bpm;
    case LocationRole:
        return r.location;
    case SourceRole:
        return r.source;
    default:
        return {};
    }
}

void QmlSuggestionsModel::setCount(int c) {
    if (c == m_count || c <= 0) {
        return;
    }
    m_count = c;
    Q_EMIT countChanged();
    refresh();
}

void QmlSuggestionsModel::refresh() {
    Library* pLibrary = QmlLibraryProxy::get();
    if (!pLibrary || !pLibrary->trackCollectionManager()) {
        return;
    }
    auto* pInternal = pLibrary->trackCollectionManager()->internalCollection();
    if (!pInternal) {
        return;
    }
    QSqlDatabase db = pInternal->database();
    if (!db.isOpen()) {
        return;
    }

    QVector<Row> next;
    next.reserve(m_count);
    const QSet<int> loaded = currentlyLoadedTrackIds();
    QSet<int> seen = loaded;

    auto addRow = [&](const QSqlQuery& q, const QString& src) {
        if (next.size() >= m_count) {
            return;
        }
        const int id = q.value(0).toInt();
        if (id <= 0 || seen.contains(id)) {
            return;
        }
        seen.insert(id);
        Row r;
        r.trackId = id;
        r.title = q.value(1).toString();
        r.artist = q.value(2).toString();
        r.durationSec = static_cast<int>(q.value(3).toDouble());
        r.bpm = q.value(4).toDouble();
        r.location = q.value(5).toString();
        r.source = src;
        next.append(r);
    };

    // 1) AutoDJ queue picks. The queue is a hidden playlist, joined via
    //    PlaylistTracks; we order by playlist position so the first suggestion
    //    is what AutoDJ would actually play next.
    const int autoDjId = pInternal->getPlaylistDAO().getPlaylistIdFromName(
            AUTODJ_TABLE);
    if (autoDjId >= 0) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
                "SELECT library.id, library.title, library.artist, "
                "       library.duration, library.bpm, "
                "       (track_locations.directory || '/' || track_locations.filename) AS path "
                "FROM PlaylistTracks "
                "JOIN library ON library.id = PlaylistTracks.track_id "
                "JOIN track_locations ON track_locations.id = library.location "
                "WHERE PlaylistTracks.playlist_id = :pl "
                "  AND library.mixxx_deleted = 0 "
                "ORDER BY PlaylistTracks.position ASC LIMIT :lim"));
        q.bindValue(QStringLiteral(":pl"), autoDjId);
        q.bindValue(QStringLiteral(":lim"), m_count * 2);
        if (q.exec()) {
            while (q.next() && next.size() < m_count) {
                addRow(q, QStringLiteral("autodj"));
            }
        } else {
            kLogger.debug() << "AutoDJ suggestions query failed:" << q.lastError().text();
        }
    }

    // 2) Recently-added fallback. Skip what we've already added and what's on
    //    a deck. We over-fetch by 4× to absorb skipped rows.
    if (next.size() < m_count) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
                "SELECT library.id, library.title, library.artist, "
                "       library.duration, library.bpm, "
                "       (track_locations.directory || '/' || track_locations.filename) AS path "
                "FROM library "
                "JOIN track_locations ON track_locations.id = library.location "
                "WHERE library.mixxx_deleted = 0 AND track_locations.fs_deleted = 0 "
                "ORDER BY library.datetime_added DESC LIMIT :lim"));
        q.bindValue(QStringLiteral(":lim"), m_count * 4);
        if (q.exec()) {
            while (q.next() && next.size() < m_count) {
                addRow(q, QStringLiteral("recent"));
            }
        }
    }

    beginResetModel();
    m_rows = std::move(next);
    endResetModel();
}

void QmlSuggestionsModel::loadOntoNextDeck(int row) {
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const QString location = m_rows.at(row).location;
    if (location.isEmpty()) {
        return;
    }
    auto* pPm = QmlPlayerManagerProxy::get();
    if (!pPm) {
        return;
    }
    pPm->slotLoadLocationIntoNextAvailableDeck(location, /*play=*/false);
    refresh();
}

void QmlSuggestionsModel::appendToAutoDj(int row) {
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    Library* pLibrary = QmlLibraryProxy::get();
    if (!pLibrary || !pLibrary->trackCollectionManager()) {
        return;
    }
    auto* pInternal = pLibrary->trackCollectionManager()->internalCollection();
    if (!pInternal) {
        return;
    }
    PlaylistDAO& pdao = pInternal->getPlaylistDAO();
    int autoDjId = pdao.getPlaylistIdFromName(AUTODJ_TABLE);
    if (autoDjId < 0) {
        autoDjId = pdao.createPlaylist(AUTODJ_TABLE, PlaylistDAO::PLHT_AUTO_DJ);
    }
    if (autoDjId < 0) {
        return;
    }
    pdao.appendTrackToPlaylist(TrackId(m_rows.at(row).trackId), autoDjId);
    refresh();
}

} // namespace qml
} // namespace mixxx
