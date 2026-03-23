#pragma once

#include "library/youtube/youtubeservice.h"
#include "sources/soundsource.h"
#include "sources/soundsourceprovider.h"

namespace mixxx {

class SoundSourceYouTube : public SoundSource {
  public:
    explicit SoundSourceYouTube(const QUrl& url);
    ~SoundSourceYouTube() override;

    void close() override;

    void onSponsorSegmentsFetched(const QString& videoId, const QList<mixxx::SponsorSegment>& segments);

  protected:
    ReadableSampleFrames readSampleFramesClamped(
            const WritableSampleFrames& sampleFrames) override;

    OpenResult tryOpen(
            OpenMode mode,
            const OpenParams& params) override;

  private:
    QList<mixxx::SponsorSegment> m_sponsorSegments;
    bool m_segmentsLoaded;
};

class SoundSourceProviderYouTube : public SoundSourceProvider {
  public:
    static const QString kDisplayName;

    QString getDisplayName() const override {
        return kDisplayName;
    }
    QStringList getSupportedFileTypes() const override {
        return {"youtube"};
    }
    SoundSourceProviderPriority getPriorityHint(const QString& /*supportedFileType*/) const override {
        return SoundSourceProviderPriority::Higher;
    }
    SoundSourcePointer newSoundSource(const QUrl& url) override {
        return std::make_shared<SoundSourceYouTube>(url);
    }
};

} // namespace mixxx
