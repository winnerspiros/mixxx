#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include "control/controlproxy.h"
#include "library/youtube/youtubeservice.h"
#include "track/track_decl.h"

class PlayerManager;

namespace mixxx {

/// Watches every deck for tracks that come from the YouTube cache directory
/// and, while they play, automatically seeks past SponsorBlock-flagged
/// segments (sponsorships, intros, outros, etc.).
///
/// Design:
///  * One controller instance owns the whole feature; it discovers the set of
///    decks via `[App]/num_decks` and dynamically grows when the count changes.
///  * For each `[ChannelN]`, it listens to `track_loaded` (well, the
///    PlayerManager's `trackChanged` signal) and `playposition`.
///  * When the new track's file lives in the YouTube cache, segments are looked
///    up from the sidecar JSON written at download time, falling back to the
///    SponsorBlock public API. Segments are kept in memory keyed by deck group.
///  * On a `playposition` change that crosses the start of a known segment,
///    the controller writes `playposition` to the segment's end → instant skip.
///
/// The controller does **nothing** for tracks that are not from the YouTube
/// cache, and it is safe to compile in unconditionally — when `NETWORKAUTH` is
/// off it just observes nothing.
class SponsorBlockController : public QObject {
    Q_OBJECT
  public:
    explicit SponsorBlockController(
            const QString& cacheDir, QObject* parent = nullptr);
    ~SponsorBlockController() override;

    /// Override the YouTube cache directory used for resolving sidecars.
    void setCacheDir(const QString& cacheDir) {
        m_cacheDir = cacheDir;
    }

  private slots:
    void onTrackChanged(
            const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack);
    void onSponsorSegmentsFetched(
            const QString& videoId, const QList<SponsorSegment>& segments);

  private:
    struct DeckState {
        QString videoId;
        QList<SponsorSegment> segments;
        ControlProxy* pPlayPosition = nullptr;
        // Track duration (sec). playposition is normalised 0..1, so we need
        // duration to convert SponsorBlock's seconds back to a position.
        double durationSec = 0.0;
    };

    void ensureDeckTracked(const QString& group);
    void clearDeck(const QString& group);
    void onPlayPositionChanged(const QString& group, double pos);
    /// Read sidecar JSON written by yt-dlp's --write-info-json (`<id>.info.json`)
    /// next to a sibling `<id>.sponsorblock.json` if we have one. Returns true
    /// if segments were applied immediately; false if the controller had to
    /// fetch from the network (callback will populate later).
    bool tryLoadSegmentsFromSidecar(
            const QString& group, const QString& videoId);

    QString m_cacheDir;
    QHash<QString, DeckState> m_decks; // key = "[Channel1]" etc.
    YouTubeService m_service;
};

} // namespace mixxx
