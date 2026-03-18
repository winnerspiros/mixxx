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
#include <QDir>
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
    return isatty(fileno(stderr)) != 0;
#else
    if (!_isatty(_fileno(stderr))) {
        return false;
    }

    // Check if terminal is known to support ANSI colors
    QString term = QProcessEnvironment::systemEnvironment().value("TERM");
    return term == u"alacritty"_s || term == u"ansi"_s || term == u"cygwin"_s ||
            term == u"linux"_s || term.startsWith(u"screen"_s) ||
            term.startsWith(u"xterm"_s) || term.startsWith(u"vt100"_s) ||
            term.startsWith(u"rxvt"_s) || term.endsWith(u"color"_s);
#endif
    return false;
}

} // namespace

CmdlineArgs::CmdlineArgs()
        : m_qml(false),
          m_awareOfRisk(false),
          m_startInFullscreen(false),
          m_startAutoDJ(false),
          m_rescanLibrary(false),
          m_settingsPathSet(false),
          m_useLegacyVuMeter(false),
          m_useLegacySpinny(false),
          m_controllerDebug(false),
          m_controllerPreviewScreens(false),
          m_controllerAbortOnWarning(false),
          m_developer(false),
          m_safeMode(false),
          m_useColors(calcUseColorsAuto()),
          m_debugAssertBreak(false),
          m_logLevel(kLogLevelDefault),
          m_logFlushLevel(kLogFlushLevelDefault),
          m_logMaxFileSize(kLogMaxFileSizeDefault),
          m_scaleFactor(1.0),
          m_parseForUserFeedbackRequired(false) {
// We are not ready to switch to XDG folders under Linux, so keeping
// /home/jules/.mixxx as preferences folder. see #8090
#if defined(__LINUX__) || defined(__BSD__)
#ifdef MIXXX_SETTINGS_PATH
    m_settingsPath = QDir::homePath().append("/").append(MIXXX_SETTINGS_PATH);
#else
#error "MIXXX_SETTINGS_PATH NOT defined"
#endif
#elif defined(Q_OS_IOS)
    m_settingsPath =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    .append("/Library/Application Support/Mixxx");
#else
    m_settingsPath =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
    if (!m_settingsPath.endsWith("/")) {
        m_settingsPath.append("/");
    }

    m_resourcePath = QCoreApplication::applicationDirPath();
}

bool CmdlineArgs::parse(int argc, char** argv) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }
    return parse(arguments, ParseMode::Initial);
}

void CmdlineArgs::parseForUserFeedback() {
    // restart parsing
    parse(QCoreApplication::arguments(), ParseMode::ForUserFeedback);
}

bool CmdlineArgs::parseLogLevel(const QString& value, LogLevel* pLogLevel) {
    if (value == QLatin1String("critical")) {
        *pLogLevel = LogLevel::Critical;
    } else if (value == QLatin1String("warning")) {
        *pLogLevel = LogLevel::Warning;
    } else if (value == QLatin1String("info")) {
        *pLogLevel = LogLevel::Info;
    } else if (value == QLatin1String("debug")) {
        *pLogLevel = LogLevel::Debug;
    } else if (value == QLatin1String("trace")) {
        *pLogLevel = LogLevel::Trace;
    } else {
        *pLogLevel = LogLevel::Warning;
        return false;
    }
    return true;
}

bool CmdlineArgs::parse(const QStringList& arguments, ParseMode mode) {
    const bool forUserFeedback = mode == ParseMode::ForUserFeedback;
    QCommandLineParser parser;

    if (forUserFeedback) {
        parser.setApplicationDescription(
                QCoreApplication::translate("CmdlineArgs", "Mixxx - Digital DJ software"));
    }

    const QCommandLineOption fullScreen(QStringLiteral("fullScreen"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Starts Mixxx in full-screen mode")
                            : QString());
    const QCommandLineOption fullScreenDeprecated(QStringLiteral("f"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Starts Mixxx in full-screen mode")
                            : QString());
    parser.addOption(fullScreen);
    parser.addOption(fullScreenDeprecated);

    const QCommandLineOption locale(QStringLiteral("locale"),
            forUserFeedback
                    ? QCoreApplication::translate("CmdlineArgs",
                              "Use a specific locale for GUI (e.g. de_DE)")
                    : QString(),
            QStringLiteral("LOCALE"));
    parser.addOption(locale);

    const QCommandLineOption startAutoDJ(QStringLiteral("startAutoDJ"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Starts AutoDJ on start-up")
                            : QString());
    parser.addOption(startAutoDJ);

    const QCommandLineOption rescanLibrary(QStringLiteral("rescan-library"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Rescan library on start-up")
                            : QString());
    parser.addOption(rescanLibrary);

    const QCommandLineOption settingsPath(QStringLiteral("settingsPath"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Use a specific folder for settings and "
                                      "library database")
                            : QString(),
            QStringLiteral("PATH"));
    const QCommandLineOption settingsPathDeprecated(QStringLiteral("p"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Use a specific folder for settings and "
                                      "library database")
                            : QString(),
            QStringLiteral("PATH"));
    parser.addOption(settingsPath);
    parser.addOption(settingsPathDeprecated);

    const QCommandLineOption resourcePath(QStringLiteral("resourcePath"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Use a specific folder for resources "
                                      "(e.g. skins)")
                            : QString(),
            QStringLiteral("PATH"));
    const QCommandLineOption resourcePathDeprecated(QStringLiteral("q"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Use a specific folder for resources "
                                      "(e.g. skins)")
                            : QString(),
            QStringLiteral("PATH"));
    parser.addOption(resourcePath);
    parser.addOption(resourcePathDeprecated);

    const QCommandLineOption timelinePath(QStringLiteral("timelinePath"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Path to a file for logging stats on "
                                      "track features")
                            : QString(),
            QStringLiteral("PATH"));
    const QCommandLineOption timelinePathDeprecated(QStringLiteral("t"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Path to a file for logging stats on "
                                      "track features")
                            : QString(),
            QStringLiteral("PATH"));
    parser.addOption(timelinePath);
    parser.addOption(timelinePathDeprecated);

    const QCommandLineOption enableLegacyVuMeter(
            QStringLiteral("enable-legacy-vumeter"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Use the legacy VU meter behavior")
                            : QString());
    parser.addOption(enableLegacyVuMeter);

    const QCommandLineOption enableLegacySpinny(
            QStringLiteral("enable-legacy-spinny"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Use the legacy spinny behavior")
                            : QString());
    parser.addOption(enableLegacySpinny);

    const QCommandLineOption controllerDebug(QStringLiteral("controllerDebug"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Log all data sent to/from controllers")
                            : QString());
    const QCommandLineOption controllerDebugDeprecated(
            QStringLiteral("debugHID"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Log all data sent to/from controllers")
                            : QString());
    parser.addOption(controllerDebug);
    parser.addOption(controllerDebugDeprecated);

    const QCommandLineOption controllerAbortOnWarning(
            QStringLiteral("controllerAbortOnWarning"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Abort on controller script warnings")
                            : QString());
    parser.addOption(controllerAbortOnWarning);

    const QCommandLineOption developer(QStringLiteral("developer"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Enables developer mode (extra logging, "
                                      "checks, etc.)")
                            : QString());
    parser.addOption(developer);

    const QCommandLineOption qml(QStringLiteral("new-ui"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Enables the experimental QML UI")
                            : QString());
    const QCommandLineOption qmlDeprecated(QStringLiteral("qml"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Enables the experimental QML UI")
                            : QString());
    parser.addOption(qml);
    parser.addOption(qmlDeprecated);

    const QCommandLineOption awareOfRisk(QStringLiteral("aware-of-risk"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Indicate that you are aware of the "
                                      "risks of using experimental features")
                            : QString());
    parser.addOption(awareOfRisk);

    const QCommandLineOption safeMode(QStringLiteral("safeMode"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Starts Mixxx in safe mode")
                            : QString());
    const QCommandLineOption safeModeDeprecated(QStringLiteral("s"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Starts Mixxx in safe mode")
                            : QString());
    parser.addOption(safeMode);
    parser.addOption(safeModeDeprecated);

    const QCommandLineOption color(QStringLiteral("color"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Whether to use colors in console output "
                                      "(always, never, auto)")
                            : QString(),
            QStringLiteral("auto/always/never"));
    parser.addOption(color);

    const QCommandLineOption logLevelOption(QStringLiteral("log-level"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Sets the logging level.\n"
                                      "trace    - Above + Profiling messages\n"
                                      "debug    - Above + Debug/Developer messages\n"
                                      "info     - Above + Informational messages\n"
                                      "warning  - Above + Warnings (default)\n"
                                      "critical - Critical/Fatal only")
                            : QString(),
            QStringLiteral("level"));
    QCommandLineOption logLevelDeprecated(QStringLiteral("logLevel"), logLevelOption.description());
    logLevelDeprecated.setFlags(QCommandLineOption::HiddenFromHelp);
    logLevelDeprecated.setValueName(logLevelOption.valueName());
    parser.addOption(logLevelOption);
    parser.addOption(logLevelDeprecated);

    const QCommandLineOption logFlushLevel(QStringLiteral("log-flush-level"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Sets the the logging level at which the log buffer is "
                                      "flushed to mixxx.log. <level> is one of the values defined "
                                      "at --log-level above.")
                            : QString(),
            QStringLiteral("level"));
    QCommandLineOption logFlushLevelDeprecated(
            QStringLiteral("logFlushLevel"), logLevelOption.description());
    logFlushLevelDeprecated.setFlags(QCommandLineOption::HiddenFromHelp);
    logFlushLevelDeprecated.setValueName(logFlushLevel.valueName());
    parser.addOption(logFlushLevel);
    parser.addOption(logFlushLevelDeprecated);

    const QCommandLineOption logMaxFileSize(QStringLiteral("log-max-file-size"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Sets the maximum file size of the "
                                      "mixxx.log file in bytes. "
                                      "Use -1 for unlimited. The default is "
                                      "100 MB as 1e5 or 100000000.")
                            : QString(),
            QStringLiteral("bytes"));
    logFlushLevelDeprecated.setFlags(QCommandLineOption::HiddenFromHelp);
    logFlushLevelDeprecated.setValueName(logFlushLevel.valueName());
    parser.addOption(logMaxFileSize);

    QCommandLineOption debugAssertBreak(QStringLiteral("debug-assert-break"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Breaks (SIGINT) Mixxx, if a DEBUG_ASSERT evaluates to "
                                      "false. Under a debugger you can continue afterwards.")
                            : QString());
    QCommandLineOption debugAssertBreakDeprecated(
            QStringLiteral("debugAssertBreak"), debugAssertBreak.description());
    debugAssertBreakDeprecated.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(debugAssertBreak);
    parser.addOption(debugAssertBreakDeprecated);

    const QCommandLineOption styleOption(QStringLiteral("style"),
            forUserFeedback
                    ? QCoreApplication::translate("CmdlineArgs",
                              "Overrides the default application GUI style. Possible values: %1")
                              .arg(QStyleFactory::keys().join(QStringLiteral(", ")))
                    : QString(),
            QStringLiteral("style"));
    parser.addOption(styleOption);

    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("file"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Load the specified music file(s) at start-up. Each file "
                                      "you specify will be loaded into the next virtual deck.")
                            : QString());

    const QCommandLineOption controllerPreviewScreens(QStringLiteral("controller-preview-screens"),
            forUserFeedback ? QCoreApplication::translate("CmdlineArgs",
                                      "Preview rendered controller screens in the Setting windows.")
                            : QString());
    parser.addOption(controllerPreviewScreens);

    if (forUserFeedback) {
        // We know form the first path, that there will be likely an error message, check again.
        // This is not the case if the user uses a Qt internal option that is unknown
        // in the first path
        puts(""); // Add a blank line to make the parser output more visible
                  // This call does not return and calls exit() in case of help or an parser error
        parser.process(arguments);
        return true;
    }

    // From here, we are in in the initial parse mode
    DEBUG_ASSERT(mode == ParseMode::Initial);

    // process all arguments
    if (!parser.parse(arguments)) {
        // we have an misspelled argument or one that is processed
        // in the not yet initialized QCoreApplication
        m_parseForUserFeedbackRequired = true;
    }

    if (parser.isSet(versionOption) ||
            parser.isSet(helpOption)
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            || parser.isSet(QStringLiteral("help-all"))
#endif
    ) {
        m_parseForUserFeedbackRequired = true;
    }

    m_startInFullscreen = parser.isSet(fullScreen) || parser.isSet(fullScreenDeprecated);

    if (parser.isSet(locale)) {
        m_locale = parser.value(locale);
    }

    if (parser.isSet(startAutoDJ)) {
        m_startAutoDJ = true;
    }

    if (parser.isSet(rescanLibrary)) {
        m_rescanLibrary = true;
    }

    if (parser.isSet(settingsPath)) {
        m_settingsPath = parser.value(settingsPath);
        if (!m_settingsPath.endsWith("/")) {
            m_settingsPath.append("/");
        }
        m_settingsPathSet = true;
    } else if (parser.isSet(settingsPathDeprecated)) {
        m_settingsPath = parser.value(settingsPathDeprecated);
        if (!m_settingsPath.endsWith("/")) {
            m_settingsPath.append("/");
        }
        m_settingsPathSet = true;
    }

    if (parser.isSet(resourcePath)) {
        m_resourcePath = parser.value(resourcePath);
    } else if (parser.isSet(resourcePathDeprecated)) {
        m_resourcePath = parser.value(resourcePathDeprecated);
    }

    if (parser.isSet(timelinePath)) {
        m_timelinePath = parser.value(timelinePath);
    } else if (parser.isSet(timelinePathDeprecated)) {
        m_timelinePath = parser.value(timelinePathDeprecated);
    }

    m_useLegacyVuMeter = parser.isSet(enableLegacyVuMeter);
    m_useLegacySpinny = parser.isSet(enableLegacySpinny);
    m_controllerDebug = parser.isSet(controllerDebug) || parser.isSet(controllerDebugDeprecated);
    m_controllerPreviewScreens = parser.isSet(controllerPreviewScreens);
    m_controllerAbortOnWarning = parser.isSet(controllerAbortOnWarning);
    m_developer = parser.isSet(developer);
    m_qml = parser.isSet(qml);
    if (parser.isSet(qmlDeprecated)) {
        m_qml |= true;
        qWarning() << "The argument '--qml' is deprecated and will be soon "
                      "removed. Please use '--new-ui' instead!";
    }
    m_awareOfRisk = parser.isSet(awareOfRisk);
    m_safeMode = parser.isSet(safeMode) || parser.isSet(safeModeDeprecated);
    m_debugAssertBreak = parser.isSet(debugAssertBreak) || parser.isSet(debugAssertBreakDeprecated);

    m_musicFiles = parser.positionalArguments();

    if (parser.isSet(logLevelOption)) {
        if (!parseLogLevel(parser.value(logLevelOption), &m_logLevel)) {
            fputs("\nlog-level wasn't 'trace', 'debug', 'info', 'warning', or 'critical'!\n"
                  "Mixxx will only print warnings and critical messages to the console.\n",
                    stdout);
        }
    } else if (parser.isSet(logLevelDeprecated)) {
        if (!parseLogLevel(parser.value(logLevelDeprecated), &m_logLevel)) {
            fputs("\nlogLevel wasn't 'trace', 'debug', 'info', 'warning', or 'critical'!\n"
                  "Mixxx will only print warnings and critical messages to the console.\n",
                    stdout);
        }
    } else {
        if (m_developer) {
            m_logLevel = LogLevel::Debug;
        }
    }

    if (parser.isSet(logFlushLevel)) {
        if (!parseLogLevel(parser.value(logFlushLevel), &m_logFlushLevel)) {
            fputs("\nlog-flush-level wasn't 'trace', 'debug', 'info', 'warning', or 'critical'!\n"
                  "Mixxx will only flush output after a critical message.\n",
                    stdout);
        }
    } else if (parser.isSet(logFlushLevelDeprecated)) {
        if (!parseLogLevel(parser.value(logFlushLevelDeprecated), &m_logFlushLevel)) {
            fputs("\nlogFlushLevel wasn't 'trace', 'debug', 'info', 'warning', or 'critical'!\n"
                  "Mixxx will only flush output after a critical message.\n",
                    stdout);
        }
    }

    if (parser.isSet(logMaxFileSize)) {
        QString strLogMaxFileSize = parser.value(logMaxFileSize);
        bool ok = false;
        // We parse it as double to also support exponential notation
        m_logMaxFileSize = static_cast<qint64>(strLogMaxFileSize.toDouble(&ok));
        if (!ok) {
            fputs("\nFailed to parse log-max-file-size.\n", stdout);
            return false;
        }
    }

    // set colors
    if (parser.value(color).compare(QLatin1String("always"), Qt::CaseInsensitive) == 0) {
        m_useColors = true;
    } else if (parser.value(color).compare(QLatin1String("never"), Qt::CaseInsensitive) == 0) {
        m_useColors = false;
    } else if (parser.value(color).compare(QLatin1String("auto"), Qt::CaseInsensitive) != 0) {
        fputs("Unknown argument for for color.\n", stdout);
    }

    if (parser.isSet(styleOption)) {
        m_styleName = parser.value(styleOption);
    }

    return true;
}

} // namespace mixxx
