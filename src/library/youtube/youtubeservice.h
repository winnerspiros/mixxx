#pragma once

#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

namespace mixxx {

struct SponsorSegment {
    double start;
    double end;
    QString category;
};

class YouTubeService : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeService(QObject* parent = nullptr);

    void fetchSponsorSegments(const QString& videoId);

  signals:
    void sponsorSegmentsFetched(const ::QString& videoId, const ::QList<::mixxx::SponsorSegment>& segments);

  private:
    // Implementation details
};

} // namespace mixxx
