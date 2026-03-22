#include "sources/youtube/soundsourceyoutube.h"

#include <QObject>
#include <QProcess>

#include "library/youtube/youtubeservice.h"
#include "util/logger.h"

namespace mixxx {

namespace {
const Logger kLogger("SoundSourceYouTube");
} // namespace

const QString SoundSourceProviderYouTube::kDisplayName = "YouTube";

SoundSourceYouTube::SoundSourceYouTube(const QUrl& url)
        : QObject(),
          SoundSource(url),
          m_segmentsLoaded(false) {
}

SoundSourceYouTube::~SoundSourceYouTube() {
    close();
}

void SoundSourceYouTube::close() {
}

ReadableSampleFrames SoundSourceYouTube::readSampleFramesClamped(
        const WritableSampleFrames& sampleFrames) {
    return ReadableSampleFrames(sampleFrames.frameIndexRange());
}

SoundSource::OpenResult SoundSourceYouTube::tryOpen(
        OpenMode mode,
        const OpenParams& params) {
    Q_UNUSED(mode);
    Q_UNUSED(params);
    kLogger.info() << "Opening YouTube stream:" << getUrl().toString();

    QString videoId = getUrl().toString().split("v=").last();
    YouTubeService* service = new YouTubeService(this);
    QObject::connect(service, &YouTubeService::sponsorSegmentsFetched, this, &SoundSourceYouTube::onSponsorSegmentsFetched);
    service->fetchSponsorSegments(videoId);

    return SoundSource::OpenResult::Succeeded;
}

void SoundSourceYouTube::onSponsorSegmentsFetched(const ::QString& videoId, const ::QList<::mixxx::SponsorSegment>& segments) {
    Q_UNUSED(videoId);
    m_sponsorSegments = segments;
    m_segmentsLoaded = true;
    kLogger.info() << "Loaded" << segments.size() << "SponsorBlock segments";
}

} // namespace mixxx

#include "moc_soundsourceyoutube.cpp"
