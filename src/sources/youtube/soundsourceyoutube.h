#pragma once

#include "sources/soundsource.h"
#include "sources/soundsourceprovider.h"
#include "library/youtube/youtubeservice.h"

namespace mixxx {

class SoundSourceYouTube : public SoundSource {
  public:
    explicit SoundSourceYouTube(const QUrl& url);
    ~SoundSourceYouTube() override;

    void close() override;

  protected:
    ReadableSampleFrames readSampleFramesClamped(
            const WritableSampleFrames& sampleFrames) override;

    OpenResult tryOpen(
            OpenMode mode,
            const OpenParams& params) override;

  private slots:
    void onSponsorSegmentsFetched(const QString& videoId, const QList<SponsorSegment>& segments);

  private:
    QList<SponsorSegment> m_sponsorSegments;
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
    SoundSourceProviderPriority getPriorityHint(const QString& supportedFileType) const override {
        return SoundSourceProviderPriority::High;
    }
    SoundSourcePointer newSoundSource(const QUrl& url) override {
        return std::make_shared<SoundSourceYouTube>(url);
    }
};

} // namespace mixxx
