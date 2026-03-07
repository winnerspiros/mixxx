#pragma once

#include "sources/soundsource.h"
#include "sources/soundsourceprovider.h"

namespace mixxx {

class SoundSourceSpotify : public SoundSource {
public:
    explicit SoundSourceSpotify(const QUrl& url);
    ~SoundSourceSpotify() override;

    void close() override;

protected:
    ReadableSampleFrames readSampleFramesClamped(
            const WritableSampleFrames& sampleFrames) override;

    OpenResult tryOpen(
            OpenMode mode,
            const OpenParams& params) override;
};

class SoundSourceProviderSpotify : public SoundSourceProvider {
public:
    static const QString kDisplayName;

    QString getDisplayName() const override { return kDisplayName; }
    QStringList getSupportedFileTypes() const override { return {"spotify"}; }
    SoundSourceProviderPriority getPriorityHint(const QString& supportedFileType) const override { return SoundSourceProviderPriority::High; }
    SoundSourcePointer newSoundSource(const QUrl& url) override {
        return std::make_shared<SoundSourceSpotify>(url);
    }
};

} // namespace mixxx
