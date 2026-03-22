#pragma once

#include <QObject>

#include "sources/soundsource.h"

namespace mixxx {

class SoundSourceSpotify : public QObject, public SoundSource {
    Q_OBJECT
  public:
    explicit SoundSourceSpotify(const QUrl& url);
    ~SoundSourceSpotify() override = default;

    static QString getDisplayName() {
        return QStringLiteral("Spotify");
    }

    void close() override;
    ReadableSampleFrames readSampleFramesClamped(
            const WritableSampleFrames& sampleFrames) override;

    OpenResult tryOpen(
            OpenMode mode,
            const OpenParams& params) override;
};

class SoundSourceProviderSpotify : public SoundSourceProvider {
  public:
    SoundSourceProviderSpotify() = default;
    ~SoundSourceProviderSpotify() override = default;

    static const QString kDisplayName;

    QString getDisplayName() const override {
        return kDisplayName;
    }
    QStringList getSupportedFileTypes() const override {
        return {"spotify"};
    }
    SoundSourceProviderPriority getPriorityHint(
            const QString& /*supportedFileType*/) const override {
        return SoundSourceProviderPriority::Higher;
    }
    SoundSourcePointer newSoundSource(const QUrl& url) override {
        return std::make_unique<SoundSourceSpotify>(url);
    }
};

} // namespace mixxx
