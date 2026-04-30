#pragma once

#include <QList>
#include <QString>

namespace mixxx {

struct SponsorSegment;

/// Physically removes time ranges from an audio file using libavformat
/// stream-copy (no re-encoding, so no quality loss and runtime is bounded by
/// disk I/O). After cutting, the file's reported duration, beat-grid, BPM,
/// and waveform analysis all reflect the music-only length — so SponsorBlock
/// segments end up looking, sounding, and timing exactly like they were never
/// part of the track.
///
/// Returns true on success and replaces `path` in-place. Returns false on any
/// failure (in which case `path` is left untouched and the caller should
/// fall back to playback-time skipping via SponsorBlockController).
bool cutAudioRanges(const QString& path, const QList<SponsorSegment>& segments);

} // namespace mixxx
