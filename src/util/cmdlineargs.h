#pragma once

#include <QCommandLineParser>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "util/logging.h"
#include "util/singleton.h"

namespace mixxx {
class CmdlineArgs : public Singleton<CmdlineArgs> {
  public:
    enum class ParseMode {
        Initial,
        ForUserFeedback,
    };

} // namespace mixxx

using CmdlineArgs = mixxx::CmdlineArgs;

CmdlineArgs();

bool isQml() const {
    return m_qml;
}
bool isAwareOfRisk() const {
    return m_awareOfRisk;
}
bool getStartInFullscreen() const {
    return m_startInFullscreen;
}
const QString& getLocale() const {
    return m_locale;
}
bool getStartAutoDJ() const {
    return m_startAutoDJ;
}
bool getRescanLibrary() const {
    return m_rescanLibrary;
}
const QString& getSettingsPath() const {
    return m_settingsPath;
}
void setSettingsPath(const QString& path) {
    m_settingsPath = path;
    if (!m_settingsPath.endsWith("/")) {
        m_settingsPath.append("/");
    }
}
bool getSettingsPathSet() const {
    return m_settingsPathSet;
}
bool getTimelineEnabled() const {
    return !m_timelinePath.isEmpty();
}
const QString& getResourcePath() const {
    return m_resourcePath;
}
const QString& getTimelinePath() const {
    return m_timelinePath;
}
bool getUseLegacyVuMeter() const {
    return m_useLegacyVuMeter;
}
bool getUseLegacySpinny() const {
    return m_useLegacySpinny;
}
bool getControllerDebug() const {
    return m_controllerDebug;
}
bool getControllerPreviewScreens() const {
    return m_controllerPreviewScreens;
}
bool getControllerAbortOnWarning() const {
    return m_controllerAbortOnWarning;
}
bool getDeveloper() const {
    return m_developer;
}
bool getSafeMode() const {
    return m_safeMode;
}
bool useColors() const {
    return m_useColors;
}
bool getDebugAssertBreak() const {
    return m_debugAssertBreak;
}
const QStringList& getMusicFiles() const {
    return m_musicFiles;
}
mixxx::LogLevel getLogLevel() const {
    return m_logLevel;
}
mixxx::LogLevel getLogFlushLevel() const {
    return m_logFlushLevel;
}
qint64 getLogMaxFileSize() const {
    return m_logMaxFileSize;
}
double getScaleFactor() const {
    return m_scaleFactor;
}
void setScaleFactor(double scaleFactor) {
    m_scaleFactor = scaleFactor;
}
QString getStyle() const {
    return m_styleName;
}
const QString& getStyleName() const {
    return m_styleName;
}

bool parse(int argc, char** argv);
void parseForUserFeedback();
bool parse(const QStringList& arguments, ParseMode mode);

bool isParseForUserFeedbackRequired() const {
    return m_parseForUserFeedbackRequired;
}

private:
bool parseLogLevel(const QString& value, mixxx::LogLevel* pLogLevel);

bool m_qml;
bool m_awareOfRisk;
bool m_startInFullscreen;
QString m_locale;
bool m_startAutoDJ;
bool m_rescanLibrary;
QString m_settingsPath;
bool m_settingsPathSet;
QString m_resourcePath;
QString m_timelinePath;
bool m_useLegacyVuMeter;
bool m_useLegacySpinny;
bool m_controllerDebug;
bool m_controllerPreviewScreens;
bool m_controllerAbortOnWarning;
bool m_developer;
bool m_safeMode;
bool m_useColors;
bool m_debugAssertBreak;
QStringList m_musicFiles;
mixxx::LogLevel m_logLevel;
mixxx::LogLevel m_logFlushLevel;
qint64 m_logMaxFileSize;
QString m_styleName;
double m_scaleFactor;

bool m_parseForUserFeedbackRequired;
};

} // namespace mixxx

using CmdlineArgs = mixxx::CmdlineArgs;
