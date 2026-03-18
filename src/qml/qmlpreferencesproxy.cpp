#include "qmlpreferencesproxy.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QtGlobal>

#ifndef Q_OS_ANDROID
#include <QVideoFrame>
#include <QVideoFrameFormat>
#endif

#include "preferences/settingsmanager.h"

namespace mixxx {
namespace qml {

QmlPreferencesProxy::QmlPreferencesProxy(
        std::shared_ptr<mixxx::CoreServices> pCoreServices,
        QObject* parent)
        : QObject(parent),
          m_pCoreServices(pCoreServices) {
}

QmlPreferencesProxy::~QmlPreferencesProxy() {
}

void QmlPreferencesProxy::setVideoSink(QObject* videoSinkObject) {
#ifndef Q_OS_ANDROID
    // videoSinkObject is a QVideoSink, which is only available if Qt Multimedia is used.
    // On Android, we don't use Qt Multimedia.
    if (!videoSinkObject) {
        return;
    }

    // Connect the frame available signal to the video sink.
    // This is used for controller screen previews.
    QObject::connect(this,
            SIGNAL(videoFrameAvailable(QVideoFrame)),
            videoSinkObject,
            SLOT(setVideoFrame(QVideoFrame)));
#else
    Q_UNUSED(videoSinkObject);
#endif
}

} // namespace qml
} // namespace mixxx
