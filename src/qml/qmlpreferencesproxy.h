#pragma once

#include <memory>
#include <QObject>
#include <QString>

#ifndef Q_OS_ANDROID
#include <QVideoFrame>
#endif

#include "coreservices.h"

namespace mixxx {
namespace qml {

class QmlPreferencesProxy : public QObject {
    Q_OBJECT

  public:
    QmlPreferencesProxy(std::shared_ptr<mixxx::CoreServices> pCoreServices,
            QObject* parent = nullptr);
    ~QmlPreferencesProxy();

    Q_INVOKABLE void setVideoSink(QObject* videoSinkObject);

  signals:
#ifndef Q_OS_ANDROID
    void videoFrameAvailable(const QVideoFrame& frame);
#endif

  private:
    std::shared_ptr<mixxx::CoreServices> m_pCoreServices;
};

} // namespace qml
} // namespace mixxx
