#include "controllers/rendering/controllerrenderingengine.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQmlEngine>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

#include "controllers/controller.h"
#include "controllers/controllerenginethreadcontrol.h"
#include "controllers/scripting/legacy/controllerscriptenginelegacy.h"
#include "moc_controllerrenderingengine.cpp"
#include "util/cmdlineargs.h"
#include "util/logger.h"
#include "util/thread_affinity.h"
#include "util/timer.h"

// Used in the renderFrame method to properly abort the rendering and terminate the engine.
#define VERIFY_OR_TERMINATE(cond, msg) \
    VERIFY_OR_DEBUG_ASSERT(cond) {     \
        kLogger.warning() << msg;      \
        m_pThread->quit();             \
        return;                        \
    }

namespace {
const mixxx::Logger kLogger("ControllerRenderingEngine");
} // anonymous namespace

using Clock = std::chrono::steady_clock;

ControllerRenderingEngine::ControllerRenderingEngine(
        const ::LegacyControllerMapping::ScreenInfo& info,
        gsl::not_null<ControllerEngineThreadControl*> engineThreadControl)
        : QObject(),
          m_screenInfo(info),
          m_GLDataFormat(GL_RGBA),
          m_GLDataType(GL_UNSIGNED_BYTE),
          m_isValid(true),
          m_pEngineThreadControl(engineThreadControl) {
    switch (m_screenInfo.pixelFormat) {
    case QImage::Format_RGB16:
        m_GLDataFormat = GL_RGB;
        m_GLDataType = GL_UNSIGNED_SHORT_5_6_5;
        break;
    default:
        m_GLDataFormat = GL_RGBA;
        m_GLDataType = GL_UNSIGNED_BYTE;
        break;
    }
}

ControllerRenderingEngine::~ControllerRenderingEngine() {
    if (m_pThread) {
        stop();
        m_pThread->wait();
    }
}

bool ControllerRenderingEngine::event(QEvent* event) {
    return QObject::event(event);
}

bool ControllerRenderingEngine::isRunning() const {
    return m_pThread && m_pThread->isRunning();
}

void ControllerRenderingEngine::start() {
    if (m_pThread) {
        return;
    }

    m_pThread = std::make_unique<QThread>();
    m_pThread->setObjectName(QStringLiteral("ControllerRenderingEngine"));
    this->moveToThread(m_pThread.get());

    connect(m_pThread.get(), &QThread::started, this, &ControllerRenderingEngine::prepare);
    connect(m_pThread.get(), &QThread::finished, this, &ControllerRenderingEngine::finish);

    m_pThread->start();
}

bool ControllerRenderingEngine::stop() {
    if (!m_pThread || !m_pThread->isRunning()) {
        return false;
    }

    emit stopping();
    m_pThread->quit();
    return true;
}

void ControllerRenderingEngine::requestEngineSetup(std::shared_ptr<QQmlEngine> qmlEngine) {
    QMetaObject::invokeMethod(this,
            "setup",
            Qt::BlockingQueuedConnection,
            Q_ARG(std::shared_ptr<QQmlEngine>, qmlEngine));
}

void ControllerRenderingEngine::requestSendingFrameData(
        Controller* controller, const QByteArray& frame) {
    QMetaObject::invokeMethod(this,
            "send",
            Qt::QueuedConnection,
            Q_ARG(Controller*, controller),
            Q_ARG(QByteArray, frame));
}

void ControllerRenderingEngine::prepare() {
    m_context = std::make_unique<QOpenGLContext>();
    m_context->setFormat(m_quickWindow->requestedFormat());
    m_context->create();

    m_offscreenSurface = std::make_unique<QOffscreenSurface>();
    m_offscreenSurface->setFormat(m_context->format());
    m_offscreenSurface->create();

    m_context->makeCurrent(m_offscreenSurface.get());

    m_renderControl = std::make_unique<QQuickRenderControl>();
    m_quickWindow = std::make_unique<QQuickWindow>(m_renderControl.get());
    m_quickWindow->setGeometry(0, 0, m_screenInfo.size.width(), m_screenInfo.size.height());

    m_renderControl->initialize(m_context.get());

    m_nextFrameStart = Clock::now();
    QTimer::singleShot(0, this, &ControllerRenderingEngine::renderFrame);
}

void ControllerRenderingEngine::setup(std::shared_ptr<QQmlEngine> qmlEngine) {
    if (QThread::currentThread() != m_pThread.get()) {
        kLogger.warning() << "The ControllerRenderingEngine setup must be done by its own thread!";
        return;
    }
}

void ControllerRenderingEngine::finish() {
    m_fbo.reset();
    m_renderControl.reset();
    m_quickWindow.reset();
    m_offscreenSurface.reset();
    m_context.reset();
}

void ControllerRenderingEngine::renderFrame() {
    if (!m_pThread || !m_pThread->isRunning()) {
        return;
    }

    m_renderControl->polishItems();
    m_renderControl->beginFrame();
    m_renderControl->sync();
    m_renderControl->render();
    m_renderControl->endFrame();

    QImage frame = m_renderControl->grab();
    emit frameRendered(m_screenInfo, frame, QDateTime::currentDateTime());

    auto now = Clock::now();
    m_nextFrameStart += std::chrono::milliseconds(1000 / m_screenInfo.target_fps);
    if (m_nextFrameStart < now) {
        m_nextFrameStart = now;
    }

    QTimer::singleShot(std::chrono::duration_cast<std::chrono::milliseconds>(
                               m_nextFrameStart - now)
                               .count(),
            this,
            &ControllerRenderingEngine::renderFrame);
}

void ControllerRenderingEngine::send(Controller* controller, const QByteArray& frame) {
    Q_UNUSED(controller);
    Q_UNUSED(frame);
}
