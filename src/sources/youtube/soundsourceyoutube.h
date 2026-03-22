#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include "library/youtube/youtubeservice.h"
#include "sources/soundsource.h"

namespace mixxx {

class SoundSourceYouTube : public QObject, public SoundSource {
    Q_OBJECT
  public:
    explicit SoundSourceYouTube(const QUrl& url);
    ~SoundSourceYouTube() override;

    static QString getDisplayName() {
        return QStringLiteral("YouTube");
    }

    void close() override;
    ReadableSampleFrames readSampleFramesClamped(
            const WritableSampleFrames& sampleFrames) override;

    OpenResult tryOpen(
            OpenMode mode,
            const OpenParams& params) override;

  private slots:
    void onSponsorSegmentsFetched(const ::QString& videoId, const ::QList<::mixxx::SponsorSegment>& segments);

  private:
    QList<::mixxx::SponsorSegment> m_sponsorSegments;
    bool m_segmentsLoaded;
};

class SoundSourceProviderYouTube : public SoundSourceProvider {
  public:
    SoundSourceProviderYouTube() = default;
    ~SoundSourceProviderYouTube() override = default;

    static const QString kDisplayName;

    QString getDisplayName() const override {
        return kDisplayName;
    }
    QStringList getSupportedFileTypes() const override {
        return {"youtube"};
    }
    SoundSourceProviderPriority getPriorityHint(
            const QString& /*supportedFileType*/) const override {
        return SoundSourceProviderPriority::Higher;
    }
    SoundSourcePointer newSoundSource(const QUrl& url) override {
        return std::make_unique<SoundSourceYouTube>(url);
    }
};

} // namespace mixxx
