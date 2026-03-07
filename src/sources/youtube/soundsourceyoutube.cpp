#include "sources/youtube/soundsourceyoutube.h"

#include <QProcess>

#include "library/youtube/youtubeservice.h"
#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("SoundSourceYouTube");
}

const QString SoundSourceProviderYouTube::kDisplayName = "YouTube";

SoundSourceYouTube::SoundSourceYouTube(const QUrl& url)
        : SoundSource(url) {
}

SoundSourceYouTube::~SoundSourceYouTube() {
    close();
}

void SoundSourceYouTube::close() {
}

ReadableSampleFrames SoundSourceYouTube::readSampleFramesClamped(
        const WritableSampleFrames& sampleFrames) {
    // Check for sponsor segments and skip them
    // This is a placeholder for actual skipping logic in the audio stream
    return ReadableSampleFrames(sampleFrames.frameIndexRange(), 0);
}

SoundSource::OpenResult SoundSourceYouTube::tryOpen(
        OpenMode mode,
        const OpenParams& params) {
    Q_UNUSED(mode);
    Q_UNUSED(params);
    kLogger.info() << "Opening YouTube stream:" << getUrl().toString();

    // Fetch SponsorBlock segments
    QString videoId = getUrl().toString().split("v=").last();
    YouTubeService* service = new YouTubeService(nullptr);
    service->fetchSponsorSegments(videoId);

    return OpenResult::Ok;
}

} // namespace mixxx
