#include "mixxxmainwindow.h"
#if defined(__LINUX__) && !defined(__ANDROID__)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif

#include <QCheckBox>
#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QOpenGLContext>
#include <QStringBuilder>
#include <QUrl>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGLFormat>
#endif

#ifdef MIXXX_USE_QOPENGL
#include "widget/tooltipqopengl.h"
#include "widget/winitialglwidget.h"
#endif

#include "controllers/keyboard/keyboardeventfilter.h"
#include "coreservices.h"
#include "defs_urls.h"
#include "dialog/dlgabout.h"
#include "dialog/dlgdevelopertools.h"
#include "dialog/dlgkeywheel.h"
#include "moc_mixxxmainwindow.cpp"
#include "preferences/dialog/dlgpreferences.h"
#ifdef __BROADCAST__
#include "broadcast/broadcastmanager.h"
#endif
#include "control/controlindicatortimer.h"
#include "library/library.h"
#include "library/library_decl.h"
#include "library/library_prefs.h"
#ifdef __ENGINEPRIME__
#include "library/export/libraryexporter.h"
#endif
#include "library/library_prefs.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "recording/recordingmanager.h"
#include "skin/legacy/launchimage.h"
#include "skin/skinloader.h"
#include "soundio/soundmanager.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/debug.h"
#include "util/desktophelper.h"
#include "util/sandbox.h"
#include "util/scopedoverridecursor.h"
#include "util/timer.h"
#include "util/versionstore.h"
#include "waveform/guitick.h"
#include "waveform/sharedglcontext.h"
#include "waveform/visualsmanager.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/wglwidget.h"
#include "widget/wmainmenubar.h"

#ifdef __VINYLCONTROL__
#include "vinylcontrol/vinylcontrolmanager.h"
#endif

namespace {
// Detect if the desktop supports a global menu to decide whether we need to rebuild
// and reconnect the menu bar when switching to/from fullscreen mode.
// Compared to QMenuBar::isNativeMenuBar() (requires a set menu bar) and
// Qt::AA_DontUseNativeMenuBar, which may both change, this is way more reliable
// since it's rather unlikely that the Appmenu.Registrar service is unloaded/stopped
// while Mixxx is running.
// This is a reimplementation of QGenericUnixTheme > checkDBusGlobalMenuAvailable()
inline bool supportsGlobalMenu() {
#if defined(__LINUX__) && !defined(__ANDROID__) && !defined(QT_NO_DBUS)
    QDBusConnection conn = QDBusConnection::sessionBus();
    if (const auto* pIface = conn.interface()) {
        return pIface->isServiceRegistered("com.canonical.AppMenu.Registrar");
    }
    return false;
#else
    return false;
#endif
}

const ConfigKey kHideMenuBarConfigKey = ConfigKey("[Config]", "hide_menubar");
const ConfigKey kMenuBarHintConfigKey = ConfigKey("[Config]", "show_menubar_hint");
} // namespace

MixxxMainWindow::MixxxMainWindow(std::shared_ptr<mixxx::CoreServices> pCoreServices)
        : m_pCoreServices(pCoreServices),
          m_pCentralWidget(nullptr),
          m_pLaunchImage(nullptr),
#ifndef __APPLE__
          m_pMenuBar(nullptr),
#endif
          m_pTouchShift(pCoreServices->getTouchShift()),
          m_pSkinLoader(std::make_unique<SkinLoader>(pCoreServices, this)),
          m_pVisualsManager(std::make_unique<VisualsManager>(pCoreServices)),
          m_pGuiTick(std::make_unique<GuiTick>()),
          m_pDeveloperTools(nullptr),
          m_pKeywheel(nullptr) {
}

MixxxMainWindow::~MixxxMainWindow() {
}
