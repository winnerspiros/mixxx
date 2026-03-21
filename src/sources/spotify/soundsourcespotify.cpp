#include "sources/spotify/soundsourcespotify.h"

#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("SoundSourceSpotify");
}

const QString SoundSourceProviderSpotify::kDisplayName = "Spotify";

SoundSourceSpotify::SoundSourceSpotify(const QUrl& url)
        : SoundSource(url) {
}

SoundSourceSpotify::~SoundSourceSpotify() {
    close();
}

void SoundSourceSpotify::close() {
}

ReadableSampleFrames SoundSourceSpotify::readSampleFramesClamped(
        const WritableSampleFrames& sampleFrames) {
    // librespot-cpp integration goes here
    return ReadableSampleFrames(sampleFrames.frameIndexRange());
}

SoundSource::OpenResult SoundSourceSpotify::tryOpen(
        OpenMode mode,
        const OpenParams& params) {
    Q_UNUSED(mode);
    Q_UNUSED(params);
    kLogger.info() << "Opening Spotify stream:" << getUrl().toString();
    return SoundSource::OpenResult::Succeeded;
}

} // namespace mixxx
