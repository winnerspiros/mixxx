#include "library/youtube/youtubefeature.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include "library/library.h"
#include "library/treeitem.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("YouTubeFeature");
}

YouTubeFeature::YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, "youtube"),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
}

void YouTubeFeature::activate() {
    kLogger.info() << "YouTube Feature Activated";
}

void YouTubeFeature::searchAndActivate(const QString& query) {
    kLogger.info() << "Searching YouTube for:" << query;

    QProcess process;
    QStringList arguments;
    arguments << "--dump-json" << "--flat-playlist" << QString("ytsearch10:%1").arg(query);

    process.start("yt-dlp", arguments);
    if (!process.waitForFinished()) {
        kLogger.critical() << "yt-dlp search failed";
        return;
    }

    QByteArray output = process.readAllStandardOutput();
    QStringList lines = QString(output).split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            QString title = obj["title"].toString();
            QString id = obj["id"].toString();
            kLogger.info() << "Found YouTube Video:" << title << "(" << id << ")";
            // Here we should add the track to a temporary collection or the main collection
        }
    }
}

TreeItemModel* YouTubeFeature::sidebarModel() const {
    return m_pSidebarModel;
}

#include "moc_youtubefeature.cpp"
