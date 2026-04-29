#include "widget/wsuggestionsbar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyle>

#include "coreservices.h"
#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "moc_wsuggestionsbar.cpp"
#include "track/trackid.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("WSuggestionsBar");
constexpr int kPollIntervalMs = 3000;
constexpr int kMaxLabelChars = 38;

QString elide(const QString& s) {
    return s.size() > kMaxLabelChars ? s.left(kMaxLabelChars - 1) + QChar(0x2026)
                                     : s;
}

QSet<int> currentlyLoadedTrackIds() {
    QSet<int> ids;
    const auto loaded = PlayerInfo::instance().getLoadedTracks();
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it) {
        if (it.value() && it.value()->getId().isValid()) {
            ids.insert(it.value()->getId().toVariant().toInt());
        }
    }
    return ids;
}
} // namespace

WSuggestionsBar::WSuggestionsBar(
        std::shared_ptr<mixxx::CoreServices> pCoreServices, QWidget* pParent)
        : QFrame(pParent),
          m_pCoreServices(std::move(pCoreServices)) {
    setObjectName(QStringLiteral("SuggestionsBar"));
    // Match the LateNight palette: pure black on OLED, faint border so the
    // strip is visible against the also-black status bar.
    setStyleSheet(QStringLiteral(
            "QFrame#SuggestionsBar { background-color: #000000; border-top: "
            "1px solid #1e1e1e; }"
            "QLabel { color: #d2d2d2; }"
            "QPushButton { background-color: #0a0a0a; color: #d2d2d2; border: "
            "1px solid #1e1e1e; border-radius: 3px; padding: 2px 8px; }"
            "QPushButton:hover { background-color: #1e1e1e; }"
            "QPushButton[autodj=\"true\"] { border: 1px solid #d09300; }"));

    m_pLayout = new QHBoxLayout(this);
    m_pLayout->setContentsMargins(6, 2, 6, 2);
    m_pLayout->setSpacing(4);
    m_pHeader = new QLabel(tr("UP NEXT"), this);
    QFont headerFont = m_pHeader->font();
    headerFont.setBold(true);
    m_pHeader->setFont(headerFont);
    m_pLayout->addWidget(m_pHeader);
    m_pLayout->addStretch(1);

    m_refreshTimer.setInterval(kPollIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &WSuggestionsBar::refresh);

    // First population: defer to an event-loop tick so CoreServices has
    // fully initialised by the time the first SQL query runs.
    QTimer::singleShot(0, this, &WSuggestionsBar::refresh);
}

void WSuggestionsBar::showEvent(QShowEvent* event) {
    QFrame::showEvent(event);
    m_refreshTimer.start();
    refresh();
}

void WSuggestionsBar::hideEvent(QHideEvent* event) {
    QFrame::hideEvent(event);
    m_refreshTimer.stop();
}

QList<WSuggestionsBar::Pick> WSuggestionsBar::queryPicks(int limit) const {
    QList<Pick> picks;
    if (!m_pCoreServices) {
        return picks;
    }
    auto pLibrary = m_pCoreServices->getLibrary();
    if (!pLibrary || !pLibrary->trackCollectionManager()) {
        return picks;
    }
    auto* pInternal = pLibrary->trackCollectionManager()->internalCollection();
    if (!pInternal) {
        return picks;
    }
    QSqlDatabase db = pInternal->database();
    if (!db.isOpen()) {
        return picks;
    }
    const QSet<int> loaded = currentlyLoadedTrackIds();
    QSet<int> seen = loaded;

    auto consume = [&](QSqlQuery& q, bool fromAutoDj) {
        while (q.next() && picks.size() < limit) {
            const int id = q.value(0).toInt();
            if (id <= 0 || seen.contains(id)) {
                continue;
            }
            seen.insert(id);
            Pick p;
            p.trackId = id;
            p.title = q.value(1).toString();
            p.artist = q.value(2).toString();
            p.location = q.value(3).toString();
            p.fromAutoDj = fromAutoDj;
            picks.append(p);
        }
    };

    const int autoDjId = pInternal->getPlaylistDAO().getPlaylistIdFromName(
            AUTODJ_TABLE);
    if (autoDjId >= 0) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
                "SELECT library.id, library.title, library.artist, "
                "       track_locations.location "
                "FROM PlaylistTracks "
                "JOIN library ON library.id = PlaylistTracks.track_id "
                "JOIN track_locations ON track_locations.id = library.location "
                "WHERE PlaylistTracks.playlist_id = :pl "
                "  AND library.mixxx_deleted = 0 "
                "ORDER BY PlaylistTracks.position ASC LIMIT :lim"));
        q.bindValue(QStringLiteral(":pl"), autoDjId);
        q.bindValue(QStringLiteral(":lim"), limit * 2);
        if (q.exec()) {
            consume(q, /*fromAutoDj=*/true);
        } else {
            kLogger.debug() << "AutoDJ suggestion query failed:"
                            << q.lastError().text();
        }
    }

    if (picks.size() < limit) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
                "SELECT library.id, library.title, library.artist, "
                "       track_locations.location "
                "FROM library "
                "JOIN track_locations ON track_locations.id = library.location "
                "WHERE library.mixxx_deleted = 0 AND track_locations.fs_deleted = 0 "
                "ORDER BY library.datetime_added DESC LIMIT :lim"));
        q.bindValue(QStringLiteral(":lim"), limit * 4);
        if (q.exec()) {
            consume(q, /*fromAutoDj=*/false);
        }
    }
    return picks;
}

void WSuggestionsBar::loadOntoNextDeck(const Pick& pick) {
    if (!m_pCoreServices || pick.location.isEmpty()) {
        return;
    }
    auto pPm = m_pCoreServices->getPlayerManager();
    if (!pPm) {
        return;
    }
    pPm->slotLoadLocationIntoNextAvailableDeck(pick.location, /*play=*/false);
    refresh();
}

void WSuggestionsBar::appendToAutoDj(const Pick& pick) {
    if (!m_pCoreServices || pick.trackId <= 0) {
        return;
    }
    auto pLibrary = m_pCoreServices->getLibrary();
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
    pdao.appendTrackToPlaylist(TrackId(QVariant(pick.trackId)), autoDjId);
    refresh();
}

void WSuggestionsBar::rebuildButtons(const QList<Pick>& picks) {
    // Drop existing pills (header label + trailing stretch are kept).
    qDeleteAll(m_pills);
    m_pills.clear();

    // Insert pills between the header (index 0) and the trailing stretch.
    int insertAt = 1;
    for (const Pick& pick : picks) {
        QString text = pick.title.isEmpty() ? tr("(untitled)") : pick.title;
        if (!pick.artist.isEmpty()) {
            text = pick.artist + QStringLiteral(" — ") + text;
        }
        auto* btn = new QPushButton(elide(text), this);
        btn->setToolTip(tr("Click: load on next deck. Right-click: queue in AutoDJ.\n%1")
                        .arg(text));
        btn->setProperty("autodj", pick.fromAutoDj);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(false);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QPushButton::clicked, this, [this, pick]() {
            loadOntoNextDeck(pick);
        });
        connect(btn,
                &QWidget::customContextMenuRequested,
                this,
                [this, pick](const QPoint&) {
                    appendToAutoDj(pick);
                });
        // QPushButton style polish needs to re-evaluate the dynamic property:
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        m_pLayout->insertWidget(insertAt++, btn);
        m_pills.append(btn);
    }
}

void WSuggestionsBar::refresh() {
    rebuildButtons(queryPicks(m_pickCount));
}
