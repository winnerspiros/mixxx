#include "util/cmdlineargs.h"

#include <qglobal.h>
#include <stdio.h>
#ifndef __WINDOWS__
#include <unistd.h>
#else
#include <io.h>
#endif

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QtGlobal>

#include "config.h"
#include "defs_urls.h"
#include "sources/soundsourceproxy.h"
#include "util/assert.h"

namespace mixxx {

namespace {

bool calcUseColorsAuto() {
    // see https://no-color.org/
    if (QProcessEnvironment::systemEnvironment().contains(QLatin1String("NO_COLOR"))) {
        return false;
    }

#ifndef __WINDOWS__
    if (!isatty(fileno(stderr))) {
        return false;
    }
#else
    if (!_isatty(_fileno(stderr))) {
        return false;
    }
#endif

    // Check if terminal is known to support ANSI colors
    QString term = QProcessEnvironment::systemEnvironment().value("TERM");
    if (term == "alacritty" || term == "ansi" || term == "cygwin" || term == "linux" ||
            term.startsWith("screen") || term.startsWith("xterm") ||
            term.startsWith("vt100") || term.startsWith("rxvt") ||
            term.endsWith("color")) {
        return true;
    }
    return false;
}

bool parseLogLevel(const QString& value, mixxx::LogLevel* pLogLevel) {
    if (value == "trace") {
        *pLogLevel = mixxx::LogLevel::Trace;
    } else if (value == "debug") {
        *pLogLevel = mixxx::LogLevel::Debug;
    } else if (value == "info") {
        *pLogLevel = mixxx::LogLevel::Info;
    } else if (value == "warning") {
        *pLogLevel = mixxx::LogLevel::Warning;
    } else if (value == "critical") {
        *pLogLevel = mixxx::LogLevel::Critical;
    } else {
        return false;
    }
    return true;
}

} // namespace

CmdlineArgs::CmdlineArgs()
        : m_startInFullscreen(false), // Initialize vars
          m_startAutoDJ(false),
          m_rescanLibrary(false),
          m_controllerDebug(false),
          m_controllerAbortOnWarning(false),
          m_developer(false),
          m_qml(false),
          m_awareOfRisk(false),
          m_safeMode(false),
          m_useLegacyVuMeter(false),
          m_useLegacySpinny(false),
          m_debugAssertBreak(false),
          m_settingsPathSet(false),
          m_scaleFactor(1.0),
          m_useColors(calcUseColorsAuto()),
          m_parseForUserFeedbackRequired(false),
          m_logLevel(mixxx::kLogLevelDefault),
          m_logFlushLevel(mixxx::kLogFlushLevelDefault),
          m_logMaxFileSize(mixxx::kLogMaxFileSizeDefault) {
// We are not ready to switch to XDG folders under Linux, so keeping /home/jules/.mixxx as preferences folder. see #8090
#if defined(__LINUX__) || defined(__BSD__)
#ifdef MIXXX_SETTINGS_PATH
          m_settingsPath = QDir::homePath().append("/").append(MIXXX_SETTINGS_PATH);
#else
#error "We are not ready to switch to XDG folders under Linux"
#endif
#elif defined(Q_OS_IOS)
          m_settingsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          .append("/Library/Application Support/Mixxx");
#else
    m_settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
    if (!m_settingsPath.endsWith("/")) {
        m_settingsPath.append("/");
    }
}

bool CmdlineArgs::parse(int argc, char** argv) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }
    return parse(arguments, ParseMode::Initial);
}

void CmdlineArgs::parseForUserFeedback() {
    if (m_parseForUserFeedbackRequired) {
        parse(QCoreApplication::arguments(), ParseMode::ForUserFeedback);
    }
}

bool CmdlineArgs::parse(const QStringList& arguments, ParseMode mode) {
    bool forUserFeedback = (mode == ParseMode::ForUserFeedback);
    QCommandLineParser parser;

    if (forUserFeedback) {
        parser.setApplicationDescription(QCoreApplication::translate("CmdlineArgs",
                "Mixxx - Digital DJ software"));
    }

    const QCommandLineOption resourcePath(QStringLiteral("resourcePath"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the path where Mixxx will look for its resource files.") : QString(),
            QStringLiteral("path"));
    const QCommandLineOption resourcePathDeprecated(QStringLiteral("resource-path"), resourcePath.description());
    parser.addOption(resourcePath);
    parser.addOption(resourcePathDeprecated);

    const QCommandLineOption settingsPath(QStringLiteral("settingsPath"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the path where Mixxx will look for its settings files.") : QString(),
            QStringLiteral("path"));
    const QCommandLineOption settingsPathDeprecated(QStringLiteral("settings-path"), settingsPath.description());
    parser.addOption(settingsPath);
    parser.addOption(settingsPathDeprecated);

    const QCommandLineOption timelinePath(QStringLiteral("timelinePath"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the path where Mixxx will look for its timeline files.") : QString(),
            QStringLiteral("path"));
    const QCommandLineOption timelinePathDeprecated(QStringLiteral("timeline-path"), timelinePath.description());
    parser.addOption(timelinePath);
    parser.addOption(timelinePathDeprecated);

    const QCommandLineOption fullScreen(QStringLiteral("f"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Starts Mixxx in full-screen mode.") : QString());
    const QCommandLineOption fullScreenDeprecated(QStringLiteral("fullScreen"), fullScreen.description());
    parser.addOption(fullScreen);
    parser.addOption(fullScreenDeprecated);

    const QCommandLineOption locale(QStringLiteral("locale"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the locale for Mixxx.") : QString(),
            QStringLiteral("locale"));
    parser.addOption(locale);

    const QCommandLineOption startAutoDJ(QStringLiteral("autoDJ"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Starts Mixxx with Auto DJ enabled.") : QString());
    parser.addOption(startAutoDJ);

    const QCommandLineOption rescanLibrary(QStringLiteral("rescan"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Rescans the music library on startup.") : QString());
    parser.addOption(rescanLibrary);

    const QCommandLineOption safeMode(QStringLiteral("safeMode"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Starts Mixxx in safe mode.") : QString());
    const QCommandLineOption safeModeDeprecated(QStringLiteral("safe-mode"), safeMode.description());
    parser.addOption(safeMode);
    parser.addOption(safeModeDeprecated);

    const QCommandLineOption developer(QStringLiteral("developer"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Enables developer mode.") : QString());
    parser.addOption(developer);

    const QCommandLineOption qml(QStringLiteral("new-ui"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Enables the new QML-based UI.") : QString());
    const QCommandLineOption qmlDeprecated(QStringLiteral("qml"), qml.description());
    parser.addOption(qml);
    parser.addOption(qmlDeprecated);

    const QCommandLineOption awareOfRisk(QStringLiteral("aware-of-risk"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Confirms that you are aware of the risks of using the new UI.") : QString());
    parser.addOption(awareOfRisk);

    const QCommandLineOption controllerDebug(QStringLiteral("controllerDebug"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Enables controller debugging.") : QString());
    const QCommandLineOption controllerDebugDeprecated(QStringLiteral("controller-debug"), controllerDebug.description());
    parser.addOption(controllerDebug);
    parser.addOption(controllerDebugDeprecated);

    const QCommandLineOption controllerAbortOnWarning(QStringLiteral("controller-abort-on-warning"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Aborts if a controller warning occurs.") : QString());
    parser.addOption(controllerAbortOnWarning);

    const QCommandLineOption enableLegacyVuMeter(QStringLiteral("enable-legacy-vumeter"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Enables the legacy VU meter.") : QString());
    parser.addOption(enableLegacyVuMeter);

    const QCommandLineOption enableLegacySpinny(QStringLiteral("enable-legacy-spinny"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Enables the legacy spinny.") : QString());
    parser.addOption(enableLegacySpinny);

    const QCommandLineOption color(QStringLiteral("color"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the color mode for the console output.") : QString(),
            QStringLiteral("mode"));
    parser.addOption(color);

    const QCommandLineOption logLevel(QStringLiteral("log-level"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the logging level.") : QString(),
            QStringLiteral("level"));
    parser.addOption(logLevel);

    const QCommandLineOption logFlushLevel(QStringLiteral("log-flush-level"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the log flush level.") : QString(),
            QStringLiteral("level"));
    parser.addOption(logFlushLevel);

    const QCommandLineOption logMaxFileSize(QStringLiteral("log-max-file-size"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the maximum log file size.") : QString(),
            QStringLiteral("bytes"));
    parser.addOption(logMaxFileSize);

    const QCommandLineOption debugAssertBreak(QStringLiteral("debug-assert-break"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Breaks on debug assert.") : QString());
    parser.addOption(debugAssertBreak);

    const QCommandLineOption styleOption(QStringLiteral("style"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Sets the application style.") : QString(),
            QStringLiteral("style"));
    parser.addOption(styleOption);

    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("file"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Load music file(s) at startup.") : QString());

    const QCommandLineOption controllerPreviewScreens(QStringLiteral("controller-preview-screens"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs", "Preview controller screens.") : QString());
    parser.addOption(controllerPreviewScreens);

    if (forUserFeedback) {
        parser.process(arguments);
        return true;
    }

    if (!parser.parse(arguments)) {
        m_parseForUserFeedbackRequired = true;
    }

    if (parser.isSet(versionOption) || parser.isSet(helpOption)) {
        m_parseForUserFeedbackRequired = true;
    }

    m_startInFullscreen = parser.isSet(fullScreen) || parser.isSet(fullScreenDeprecated);
    if (parser.isSet(locale)) m_locale = parser.value(locale);
    if (parser.isSet(startAutoDJ)) m_startAutoDJ = true;
    if (parser.isSet(rescanLibrary)) m_rescanLibrary = true;

    if (parser.isSet(settingsPath)) {
        m_settingsPath = parser.value(settingsPath);
        if (!m_settingsPath.endsWith("/")) m_settingsPath.append("/");
        m_settingsPathSet = true;
    } else if (parser.isSet(settingsPathDeprecated)) {
        m_settingsPath = parser.value(settingsPathDeprecated);
        if (!m_settingsPath.endsWith("/")) m_settingsPath.append("/");
        m_settingsPathSet = true;
    }

    if (parser.isSet(resourcePath)) m_resourcePath = parser.value(resourcePath);
    else if (parser.isSet(resourcePathDeprecated)) m_resourcePath = parser.value(resourcePathDeprecated);

    if (parser.isSet(timelinePath)) m_timelinePath = parser.value(timelinePath);
    else if (parser.isSet(timelinePathDeprecated)) m_timelinePath = parser.value(timelinePathDeprecated);

    m_useLegacyVuMeter = parser.isSet(enableLegacyVuMeter);
    m_useLegacySpinny = parser.isSet(enableLegacySpinny);
    m_controllerDebug = parser.isSet(controllerDebug) || parser.isSet(controllerDebugDeprecated);
    m_controllerPreviewScreens = parser.isSet(controllerPreviewScreens);
    m_controllerAbortOnWarning = parser.isSet(controllerAbortOnWarning);
    m_developer = parser.isSet(developer);
    m_qml = parser.isSet(qml) || parser.isSet(qmlDeprecated);
    m_awareOfRisk = parser.isSet(awareOfRisk);
    m_safeMode = parser.isSet(safeMode) || parser.isSet(safeModeDeprecated);
    m_debugAssertBreak = parser.isSet(debugAssertBreak);
    m_musicFiles = parser.positionalArguments();

    if (parser.isSet(logLevel)) {
        parseLogLevel(parser.value(logLevel), &m_logLevel);
    }
    if (parser.isSet(logFlushLevel)) {
        parseLogLevel(parser.value(logFlushLevel), &m_logFlushLevel);
    }
    if (parser.isSet(logMaxFileSize)) {
        bool ok = false;
        m_logMaxFileSize = static_cast<qint64>(parser.value(logMaxFileSize).toDouble(&ok));
    }

    if (parser.isSet(color)) {
        QString colorVal = parser.value(color).toLower();
        if (colorVal == "always") m_useColors = true;
        else if (colorVal == "never") m_useColors = false;
    }

    if (parser.isSet(styleOption)) {
        m_styleName = parser.value(styleOption);
    }

    return true;
}

} // namespace mixxx
