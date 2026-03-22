#include "sources/spotify/soundsourcespotify.h"

namespace mixxx {

const QString SoundSourceProviderSpotify::kDisplayName = "Spotify";

SoundSourceSpotify::SoundSourceSpotify(const QUrl& url)
        : SoundSource(url) {
}

void SoundSourceSpotify::close() {
}

ReadableSampleFrames SoundSourceSpotify::readSampleFramesClamped(
        const WritableSampleFrames& sampleFrames) {
    return ReadableSampleFrames(sampleFrames.frameIndexRange());
}

SoundSource::OpenResult SoundSourceSpotify::tryOpen(
        OpenMode mode,
        const OpenParams& params) {
    Q_UNUSED(mode);
    Q_UNUSED(params);
    return SoundSource::OpenResult::Succeeded;
}

} // namespace mixxx

#include "moc_soundsourcespotify.cpp"
