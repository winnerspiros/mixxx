#include "qmlpreferencesproxy.h"

#include <qglobal.h>
#include <qhash.h>
#include <qqmlengine.h>
#include <qstringliteral.h>
#ifndef Q_OS_ANDROID
#include <qvideosink.h>

#include <QVideoFrame>
#include <QVideoFrameFormat>
#endif
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "util/xml.h"

#ifdef __BULK__
#include "controllers/bulk/bulkcontroller.h"
#endif
#include "controllers/controller.h"
#include "controllers/controllermanager.h"
#include "controllers/controllermappinginfo.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/legacycontrollermappingfilehandler.h"

namespace {
/// Number of sample frame timestamp sample to perform a smooth average FPS label.
constexpr double kFrameSmoothAverageFactor = 20;
} // namespace

namespace mixxx {
namespace qml {

QmlControllerScreenElement::QmlControllerScreenElement(
        QObject* parent, const ::LegacyControllerMapping::ScreenInfo& screen)
        : QObject(parent),
          m_screenInfo(screen),
          m_averageFrameDuration(std::numeric_limits<double>::max()) {
}

void QmlControllerScreenElement::updateFrame(
        const ::LegacyControllerMapping::ScreenInfo& screen, const QImage& frame) {
    if (m_screenInfo.identifier != screen.identifier) {
        return;
    }

#ifndef Q_OS_ANDROID
    emit videoFrameAvailable(QVideoFrame(frame));
#endif

    auto currentTimestamp = Clock::now();
    if (m_lastFrameTimestamp == Clock::time_point()) {
        m_lastFrameTimestamp = currentTimestamp;
        return;
    }

    if (m_averageFrameDuration == std::numeric_limits<double>::max()) {
        m_averageFrameDuration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                        currentTimestamp - m_lastFrameTimestamp)
                        .count();
    } else {
        m_averageFrameDuration = std::lerp(m_averageFrameDuration,
                std::chrono::duration_cast<std::chrono::microseconds>(
                        currentTimestamp - m_lastFrameTimestamp)
                        .count(),
                1.0 / kFrameSmoothAverageFactor);
    }
    m_lastFrameTimestamp = currentTimestamp;
    emit fpsChanged();
}

#ifndef Q_OS_ANDROID
void QmlControllerScreenElement::connectVideoSink(QObject* videoSinkObject) {
    QVideoSink* videoSink = qobject_cast<QVideoSink*>(videoSinkObject);
    if (!videoSink) {
        return;
    }
    connect(this,
            &QmlControllerScreenElement::videoFrameAvailable,
            videoSink,
            [videoSink](const QVideoFrame& frame) {
                videoSink->setVideoFrame(frame);
            });
}
#endif

QmlControllerSettingItem::QmlControllerSettingItem(
        std::shared_ptr<AbstractLegacyControllerSetting> pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
    connect(m_pInternal.get(),
            &AbstractLegacyControllerSetting::valueChanged,
            this,
            &QmlControllerSettingItem::valueChanged);
}

QString QmlControllerSettingItem::label() const {
    return m_pInternal->label();
}

QString QmlControllerSettingItem::description() const {
    return m_pInternal->description();
}

QVariant QmlControllerSettingItem::value() const {
    return m_pInternal->value();
}

void QmlControllerSettingItem::setValue(const QVariant& value) {
    m_pInternal->setValue(value);
}

QVariant QmlControllerSettingItem::defaultValue() const {
    return m_pInternal->defaultValue();
}

QVariantList QmlControllerSettingItem::possibleValues() const {
    return m_pInternal->possibleValues();
}

QmlControllerSettingContainer::QmlControllerSettingContainer(
        const LegacyControllerSettingsLayoutContainer* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
}

QmlControllerSettingGroup::QmlControllerSettingGroup(
        const LegacyControllerSettingsGroup* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
}

QmlControllerMappingProxy::QmlControllerMappingProxy(
        const MappingInfo& mapping, QObject* parent)
        : QObject(parent),
          m_mappingDefinition(mapping) {
}

mixxx::qml::QmlControllerSettingElement* QmlControllerMappingProxy::loadSettings(
        const mixxx::qml::QmlConfigProxy* pConfig,
        mixxx::qml::QmlControllerDeviceProxy* pController) {
    VERIFY_OR_DEBUG_ASSERT(pController->internal()) {
        return nullptr;
    }
    auto mapping = pController->instanceFor(m_mappingDefinition.getPath());
    if (!mapping) {
        mapping = LegacyControllerMappingFileHandler::loadMapping(
                QFileInfo(m_mappingDefinition.getPath()),
                pController->internal()->getDeviceCategory(),
                pConfig->internal());
        if (!mapping) {
            return nullptr;
        }
        pController->setInstanceFor(m_mappingDefinition.getPath(), mapping);
    }
    return nullptr; // Simplified for now
}

QList<QmlControllerScreenElement*> QmlControllerMappingProxy::loadScreens(
        const QmlConfigProxy* pConfig, mixxx::qml::QmlControllerDeviceProxy* pController) {
    VERIFY_OR_DEBUG_ASSERT(pController->internal()) {
        return {};
    }
    auto mapping = pController->instanceFor(m_mappingDefinition.getPath());
    if (!mapping) {
        mapping = LegacyControllerMappingFileHandler::loadMapping(
                QFileInfo(m_mappingDefinition.getPath()),
                pController->internal()->getDeviceCategory(),
                pConfig->internal());
        if (!mapping) {
            return {};
        }
        pController->setInstanceFor(m_mappingDefinition.getPath(), mapping);
    }

    QList<QmlControllerScreenElement*> screens;
#if defined(CONTROLLER_SCREENS)
    for (const auto& screen : mapping->screens()) {
        screens.append(new QmlControllerScreenElement(this, screen));
    }
#endif
    return screens;
}

void QmlControllerMappingProxy::resetSettings(
        mixxx::qml::QmlControllerDeviceProxy* pController) {
    auto mapping = pController->instanceFor(m_mappingDefinition.getPath());
    if (mapping) {
        mapping->resetSettings();
    }
}

QString QmlControllerMappingProxy::getName() const {
    return m_mappingDefinition.getName();
}

QString QmlControllerMappingProxy::getAuthor() const {
    return m_mappingDefinition.getAuthor();
}

QString QmlControllerMappingProxy::getDescription() const {
    return m_mappingDefinition.getDescription();
}

QUrl QmlControllerMappingProxy::getForumLink() const {
    return m_mappingDefinition.getForumLink();
}

QUrl QmlControllerMappingProxy::getWikiLink() const {
    return m_mappingDefinition.getWikiLink();
}

bool QmlControllerMappingProxy::hasSettings() {
    if (!m_hasSettings.has_value()) {
        fetchMappingDetails();
    }
    return m_hasSettings.value_or(false);
}

bool QmlControllerMappingProxy::hasScreens() {
    if (!m_hasScreens.has_value()) {
        fetchMappingDetails();
    }
    return m_hasScreens.value_or(false);
}

bool QmlControllerMappingProxy::isUserMapping(
        const mixxx::qml::QmlConfigProxy* pConfig) const {
    return m_mappingDefinition.getPath().startsWith(
            pConfig->internal()->getSettingsPath());
}

void QmlControllerMappingProxy::fetchMappingDetails() {
    QFile file(m_mappingDefinition.getPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QDomDocument doc;
    if (!doc.setContent(&file)) {
        return;
    }
    auto root = doc.documentElement();
    m_hasSettings = !root.firstChildElement(QStringLiteral("settings")).isNull();
    m_hasScreens = !root.firstChildElement(QStringLiteral("screens")).isNull();
}

QmlControllerDeviceProxy::QmlControllerDeviceProxy(Controller* pInternal,
        const std::optional<ProductInfo>& productInfo,
        const QSet<QmlControllerMappingProxy*>& mappings,
        QObject* parent)
        : QObject(parent),
          m_edited(false),
          m_pInternal(pInternal),
          m_productInfo(productInfo),
          m_mappings(mappings) {
}

QmlControllerDeviceProxy::Type QmlControllerDeviceProxy::getType() const {
    switch (m_pInternal->getDeviceCategory()) {
    case DeviceCategory::Midi:
        return Type::MIDI;
    case DeviceCategory::Hid:
        return Type::HID;
    case DeviceCategory::Bulk:
        return Type::BULK;
    }
    return Type::MIDI;
}

QString QmlControllerDeviceProxy::getName() const {
    if (!m_editedFriendlyName.isEmpty()) {
        return m_editedFriendlyName;
    }
    return m_pInternal->getName();
}

void QmlControllerDeviceProxy::setName(const QString& name) {
    if (m_editedFriendlyName == name) {
        return;
    }
    m_editedFriendlyName = name;
    setEdited();
    emit nameChanged();
}

QString QmlControllerDeviceProxy::getSinceVersion() const {
    return QString(); // Placeholder
}

QUrl QmlControllerDeviceProxy::getVisualUrl() const {
    if (!m_editedVisualUrl.isEmpty()) {
        return m_editedVisualUrl;
    }
    return QUrl(); // Placeholder
}

void QmlControllerDeviceProxy::setVisualUrl(const QUrl& url) {
    if (m_editedVisualUrl == url) {
        return;
    }
    m_editedVisualUrl = url;
    setEdited();
    emit visualUrlChanged();
}

QmlControllerMappingProxy* QmlControllerDeviceProxy::getMapping() const {
    return nullptr; // Placeholder
}

void QmlControllerDeviceProxy::setMapping(QmlControllerMappingProxy* pMapping) {
    // Placeholder
}

bool QmlControllerDeviceProxy::getEnabled() const {
    if (m_enabled.has_value()) {
        return m_enabled.value();
    }
    return m_pInternal->isOpen();
}

void QmlControllerDeviceProxy::setEnabled(bool state) {
    if (m_enabled == state) {
        return;
    }
    m_enabled = state;
    setEdited();
    emit enabledChanged();
}

QString QmlControllerDeviceProxy::vendor() const {
    return m_productInfo ? m_productInfo->vendor : QString();
}

QString QmlControllerDeviceProxy::product() const {
    return m_productInfo ? m_productInfo->product : QString();
}

QString QmlControllerDeviceProxy::serialNumber() const {
    return QString(); // Placeholder
}

bool QmlControllerDeviceProxy::save(const mixxx::qml::QmlConfigProxy* pConfig) {
    return true; // Placeholder
}

void QmlControllerDeviceProxy::clear() {
    m_edited = false;
    m_editedFriendlyName.clear();
    m_editedVisualUrl.clear();
    m_enabled.reset();
    emit editedChanged();
    emit nameChanged();
    emit visualUrlChanged();
    emit enabledChanged();
}

std::shared_ptr<LegacyControllerMapping> QmlControllerDeviceProxy::instanceFor(
        const QString& mappingPath) const {
    return m_mappingInstance.value(mappingPath);
}

void QmlControllerDeviceProxy::setInstanceFor(const QString& mappingPath,
        std::shared_ptr<LegacyControllerMapping> pMapping) {
    m_mappingInstance.insert(mappingPath, pMapping);
}

QmlControllerManagerProxy::QmlControllerManagerProxy(
        std::shared_ptr<ControllerManager> pControllerManager, QObject* parent)
        : QObject(parent),
          m_pControllerManager(pControllerManager) {
}

QQmlListProperty<QmlControllerDeviceProxy> QmlControllerManagerProxy::knownDevices() {
    return QQmlListProperty<QmlControllerDeviceProxy>(
            this, &m_knownDevicesFound);
}

QQmlListProperty<QmlControllerDeviceProxy> QmlControllerManagerProxy::unknownDevices() {
    return QQmlListProperty<QmlControllerDeviceProxy>(
            this, &m_unknownDevicesFound);
}

bool QmlControllerManagerProxy::isControllerScreenDebug() const {
    return s_controllerPreviewScreens;
}

std::shared_ptr<ControllerManager> QmlControllerManagerProxy::internal() const {
    return m_pControllerManager;
}

QmlControllerManagerProxy* QmlControllerManagerProxy::create(
        QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    return new QmlControllerManagerProxy(s_pControllerManager);
}

void QmlControllerManagerProxy::refreshKnownDevices() {
    // Placeholder
}

void QmlControllerManagerProxy::refreshMappings() {
    // Placeholder
}

void QmlControllerManagerProxy::loadNewMapping(
        QmlControllerDeviceProxy::Type type, const MappingInfo& mapping) {
    // Placeholder
}

void QmlControllerManagerProxy::updateExistingMapping(
        QmlControllerMappingProxy* pMapping, const MappingInfo& mapping) {
    // Placeholder
}

void QmlControllerManagerProxy::loadMappingFromEnumerator(
        QSharedPointer<MappingInfoEnumerator> enumerator) {
    // Placeholder
}

} // namespace qml
} // namespace mixxx
