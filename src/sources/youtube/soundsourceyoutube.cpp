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
        : SoundSource(url),
          m_segmentsLoaded(false) {
}

SoundSourceYouTube::~SoundSourceYouTube() {
    close();
}

void SoundSourceYouTube::close() {
}

ReadableSampleFrames SoundSourceYouTube::readSampleFramesClamped(
        const WritableSampleFrames& sampleFrames) {
    // Professional skipping logic:
    // If current position is within a sponsor segment, seek forward to segment end.

    // This is a simplified implementation showing the intent.
    // In a full implementation, we would integrate this into the frame reading loop.
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
    YouTubeService* service = new YouTubeService(this);
    connect(service, &YouTubeService::sponsorSegmentsFetched, this, &SoundSourceYouTube::onSponsorSegmentsFetched);
    service->fetchSponsorSegments(videoId);

    return OpenResult::Ok;
}

void SoundSourceYouTube::onSponsorSegmentsFetched(const QString& videoId, const QList<SponsorSegment>& segments) {
    Q_UNUSED(videoId);
    m_sponsorSegments = segments;
    m_segmentsLoaded = true;
    kLogger.info() << "Loaded" << segments.size() << "SponsorBlock segments";
}

} // namespace mixxx
