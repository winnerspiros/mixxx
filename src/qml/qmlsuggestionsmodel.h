#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QVector>

namespace mixxx {
namespace qml {

/// Read-only list model exposing N suggested "next track" picks to QML.
///
/// Strategy (in order of priority):
///   1. Tracks currently sitting in the AutoDJ queue (hidden playlist named
///      "Auto DJ"). This is the user's explicit upcoming queue, so it's the
///      best signal we have for "what's next".
///   2. If the queue is shorter than `count`, fill the remainder with the
///      most-recently-added library tracks that aren't already loaded on a
///      deck (so we don't suggest something the user is currently mixing).
///
/// `refresh()` re-queries the database. The QML side calls it on first show
/// and every time a deck eject happens (so the strip stays in sync after the
/// user actually loads a suggestion).
class QmlSuggestionsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    QML_NAMED_ELEMENT(SuggestionsModel)

  public:
    enum Roles {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        DurationSecRole,
        BpmRole,
        LocationRole,
        SourceRole, // "autodj" or "recent" – lets QML colour-code differently
    };

    explicit QmlSuggestionsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const {
        return m_count;
    }
    void setCount(int c);

    /// Re-query the DB. Cheap enough to call on every deck change.
    Q_INVOKABLE void refresh();

    /// Convenience: load the suggestion at `row` onto the next free deck.
    Q_INVOKABLE void loadOntoNextDeck(int row);

    /// Convenience: append the suggestion at `row` to the AutoDJ queue.
    Q_INVOKABLE void appendToAutoDj(int row);

  signals:
    void countChanged();

  private:
    struct Row {
        int trackId = -1;
        QString title;
        QString artist;
        int durationSec = 0;
        double bpm = 0.0;
        QString location;
        QString source;
    };

    QVector<Row> m_rows;
    int m_count = 4;
};

} // namespace qml
} // namespace mixxx
