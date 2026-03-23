#include "qml/qmlpreferencesproxy.h"

#include <QDir>
#include <QSet>
#include <cmath>
#include <limits>
#include <utility>

#include "controllers/controller.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/controllermanager.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "controllers/scripting/legacy/controllerscriptenginelegacy.h"
#include "qml/qmlconfigproxy.h"
#include "util/assert.h"
#include "util/logger.h"
#include "util/logging.h"
#include "util/math.h"

namespace mixxx {
namespace qml {

namespace {

const mixxx::Logger kLogger("QmlPreferencesProxy");
constexpr double kFrameSmoothAverageFactor = 10.0;

} // namespace

QmlControllerSettingElement* loadElement(
        LegacyControllerSettingsLayoutElement* element, QObject* parent) {
    auto* pItem = dynamic_cast<LegacyControllerSettingsLayoutItem*>(element);
    if (pItem) {
        auto* pElement = new QmlControllerSettingItem(pItem, parent);
        QObject::connect(pItem->setting(),
                &AbstractLegacyControllerSetting::changed,
                pElement,
                &QmlControllerSettingElement::dirtyChanged);
        return pElement;
    }
    auto* pGroup = dynamic_cast<LegacyControllerSettingsGroup*>(element);
    if (pGroup) {
        return new QmlControllerSettingGroup(pGroup, parent);
    }
    auto* pContainer = dynamic_cast<LegacyControllerSettingsLayoutContainer*>(element);
    if (pContainer) {
        return new QmlControllerSettingContainer(pContainer, parent);
    }
    DEBUG_ASSERT(!"Unreachable");
    return nullptr;
}

QmlControllerScreenElement::QmlControllerScreenElement(
        QObject* parent, const ::LegacyControllerMapping::ScreenInfo& screen)
        : QObject(parent),
          m_screenInfo(screen),
          m_averageFrameDuration(0) {
    clear();
}

void QmlControllerScreenElement::updateFrame(
        const ::LegacyControllerMapping::ScreenInfo& screen, const QImage& frame) {
    if (m_screenInfo.identifier != screen.identifier) {
        return;
    }
    auto currentTimestamp = mixxx::Time::now();
    if (m_lastFrameTimestamp == mixxx::Time::time_point()) {
        m_lastFrameTimestamp = currentTimestamp;
        return;
    }

    double averageFrameDuration;
    if (m_averageFrameDuration == std::numeric_limits<double>::max()) {
        averageFrameDuration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                        currentTimestamp - m_lastFrameTimestamp)
                        .count();
    } else {
        averageFrameDuration = std::lerp(m_averageFrameDuration,
                std::chrono::duration_cast<std::chrono::microseconds>(
                        currentTimestamp - m_lastFrameTimestamp)
                        .count(),
                1.0 / kFrameSmoothAverageFactor);
    }
    m_lastFrameTimestamp = currentTimestamp;

    if (fps() != static_cast<int>(1000000 / averageFrameDuration)) {
        m_averageFrameDuration = averageFrameDuration;
        Q_EMIT fpsChanged();
    } else {
        m_averageFrameDuration = averageFrameDuration;
    }

#ifndef Q_OS_ANDROID
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    Q_EMIT videoFrameAvailable(::QVideoFrame(frame));
#else
    auto videoFrame = ::QVideoFrame(::QVideoFrameFormat(frame.size(),
            ::QVideoFrameFormat::pixelFormatFromImageFormat(frame.format())));
    VERIFY_OR_DEBUG_ASSERT(videoFrame.isValid() &&
            videoFrame.map(::QVideoFrame::MapMode::WriteOnly)) {
        return;
    }

    std::copy(frame.bits(),
            frame.bits() + frame.sizeInBytes(),
            videoFrame.bits(0));
    videoFrame.unmap();
    Q_EMIT videoFrameAvailable(videoFrame);
#endif
#endif
}

void QmlControllerScreenElement::clear() {
    m_lastFrameTimestamp = mixxx::Time::time_point();
    m_averageFrameDuration = std::numeric_limits<double>::max();
    Q_EMIT fpsChanged();
}

QmlControllerSettingItem::QmlControllerSettingItem(
        LegacyControllerSettingsLayoutItem* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
}

QmlControllerSettingContainer::QmlControllerSettingContainer(
        const LegacyControllerSettingsLayoutContainer* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
    for (auto* pChild : pInternal->children()) {
        auto* pElement = loadElement(pChild, this);
        if (pElement) {
            m_children.append(pElement);
        }
    }
}

QmlControllerSettingGroup::QmlControllerSettingGroup(
        const LegacyControllerSettingsGroup* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
    for (auto* pChild : pInternal->children()) {
        auto* pElement = loadElement(pChild, this);
        if (pElement) {
            m_children.append(pElement);
        }
    }
}

QmlControllerMappingProxy::QmlControllerMappingProxy(
        const MappingInfo& mapping, QObject* parent)
        : QObject(parent),
          m_mappingDefinition(mapping) {
}

QmlControllerSettingElement* QmlControllerMappingProxy::loadSettings(
        const QmlConfigProxy* pConfig,
        QmlControllerDeviceProxy* pController) {
    auto pMapping = pController->instanceFor(m_mappingDefinition.getPath());
    if (!pMapping) {
        pMapping = LegacyControllerMappingFileHandler::loadMapping(
                QFileInfo(m_mappingDefinition.getPath()),
                QDir(resourceMappingsPath(pConfig->get())));
        pMapping->loadSettings(pConfig->get(), pController->getName());
        pController->setInstanceFor(m_mappingDefinition.getPath(), pMapping);
    }
    return loadElement(pMapping->getSettingsLayout(), this);
}

QList<QmlControllerScreenElement*> QmlControllerMappingProxy::loadScreens(
        const QmlConfigProxy* pConfig,
        QmlControllerDeviceProxy* pController) {
    Q_UNUSED(pConfig);
    auto pMapping = pController->instanceFor(m_mappingDefinition.getPath());
    if (!pMapping) {
        pMapping = LegacyControllerMappingFileHandler::loadMapping(
                QFileInfo(m_mappingDefinition.getPath()), QDir());
        pController->setInstanceFor(m_mappingDefinition.getPath(), pMapping);
    }
    QList<QmlControllerScreenElement*> screens;
    for (const auto& screenInfo : pMapping->getInfoScreens()) {
        screens.append(new QmlControllerScreenElement(this, screenInfo));
    }
    return screens;
}

void QmlControllerMappingProxy::resetSettings(QmlControllerDeviceProxy* pController) {
    auto pMapping = pController->instanceFor(m_mappingDefinition.getPath());
    if (pMapping) {
        pMapping->resetSettings();
    }
}

QString QmlControllerMappingProxy::getName() const {
    return m_mappingDefinition.getName();
}

QString QmlControllerMappingProxy::getAuthor() const {
    fetchMappingDetails();
    return m_mappingDefinition.getAuthor();
}

QString QmlControllerMappingProxy::getDescription() const {
    fetchMappingDetails();
    return m_mappingDefinition.getDescription();
}

QUrl QmlControllerMappingProxy::getForumLink() const {
    fetchMappingDetails();
    return QUrl(m_mappingDefinition.getForumLink());
}

QUrl QmlControllerMappingProxy::getWikiLink() const {
    fetchMappingDetails();
    return QUrl(m_mappingDefinition.getWikiLink());
}

bool QmlControllerMappingProxy::hasSettings() const {
    if (!m_hasSettings.has_value()) {
        fetchMappingDetails();
    }
    return m_hasSettings.value();
}

bool QmlControllerMappingProxy::hasScreens() const {
    if (!m_hasScreens.has_value()) {
        fetchMappingDetails();
    }
    return m_hasScreens.value();
}

bool QmlControllerMappingProxy::isUserMapping(const QmlConfigProxy* pConfig) const {
    return m_mappingDefinition.getPath().startsWith(userMappingsPath(pConfig->get()));
}

void QmlControllerMappingProxy::fetchMappingDetails() const {
    if (m_hasSettings.has_value() && m_hasScreens.has_value()) {
        return;
    }
    auto mapping = LegacyControllerMappingFileHandler::loadMapping(
            QFileInfo(m_mappingDefinition.getPath()), QDir());
    if (mapping) {
        m_hasSettings = mapping->hasSettings();
        m_hasScreens = mapping->hasScreens();
        m_mappingDefinition.setAuthor(mapping->author());
        m_mappingDefinition.setDescription(mapping->description());
        m_mappingDefinition.setForumLink(mapping->forumlink());
        m_mappingDefinition.setWikiLink(mapping->wikilink());
    } else {
        m_hasSettings = false;
        m_hasScreens = false;
        Q_EMIT mappingErrored();
    }
}

QmlControllerDeviceProxy::QmlControllerDeviceProxy(Controller* pInternal,
        const std::optional<ProductInfo>& productInfo,
        const QList<QmlControllerMappingProxy*>& mappings,
        QObject* parent)
        : QObject(parent),
          m_edited(false),
          m_pInternal(pInternal),
          m_productInfo(productInfo),
          m_mappings(mappings),
          m_pMapping(nullptr) {
    auto* pMapping = pInternal->getMapping().get();
    if (pMapping) {
        for (auto* mappingProxy : std::as_const(m_mappings)) {
            if (mappingProxy->definition().getPath() == pMapping->getPath()) {
                m_pMapping = mappingProxy;
                break;
            }
        }
    }
}

QmlControllerDeviceProxy::Type QmlControllerDeviceProxy::getType() const {
    if (m_pInternal->isMidi()) {
        return Type::MIDI;
    }
    if (m_pInternal->isHid()) {
        return Type::HID;
    }
    return Type::BULK;
}

QString QmlControllerDeviceProxy::getName() const {
    return m_pInternal->getName();
}

void QmlControllerDeviceProxy::setName(const QString& name) {
    if (m_editedFriendlyName == name) {
        return;
    }
    m_editedFriendlyName = name;
    setEdited();
    Q_EMIT nameChanged();
}

QString QmlControllerDeviceProxy::getSinceVersion() const {
    return m_pInternal->getSinceVersion();
}

QUrl QmlControllerDeviceProxy::getVisualUrl() const {
    if (m_editedVisualUrl.isValid()) {
        return m_editedVisualUrl;
    }
    return m_pInternal->getVisualUrl();
}

void QmlControllerDeviceProxy::setVisualUrl(const QUrl& url) {
    if (m_editedVisualUrl == url) {
        return;
    }
    m_editedVisualUrl = url;
    setEdited();
    Q_EMIT visualUrlChanged();
}

QmlControllerMappingProxy* QmlControllerDeviceProxy::getMapping() const {
    return m_pMapping;
}

void QmlControllerDeviceProxy::setMapping(QmlControllerMappingProxy* mapping) {
    if (m_pMapping == mapping) {
        return;
    }
    m_pMapping = mapping;
    setEdited();
    Q_EMIT mappingChanged();
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
    Q_EMIT enabledChanged();
}

QString QmlControllerDeviceProxy::vendor() const {
    if (m_productInfo.has_value()) {
        return m_productInfo.value().vendor_id;
    }
    return QString();
}

QString QmlControllerDeviceProxy::product() const {
    if (m_productInfo.has_value()) {
        return m_productInfo.value().product_id;
    }
    return QString();
}

QString QmlControllerDeviceProxy::serialNumber() const {
    return m_pInternal->getSerialNumber();
}

bool QmlControllerDeviceProxy::save(const QmlConfigProxy* pConfig) {
    auto pMappingProxy = getMapping();
    if (!pMappingProxy) {
        return false;
    }

    auto pMapping = instanceFor(pMappingProxy->definition().getPath());
    if (!pMapping) {
        pMapping = LegacyControllerMappingFileHandler::loadMapping(
                QFileInfo(pMappingProxy->definition().getPath()),
                QDir(resourceMappingsPath(pConfig->get())));
        pMapping->loadSettings(pConfig->get(), m_pInternal->getName());
        setInstanceFor(pMappingProxy->definition().getPath(), pMapping);
    }

    Q_EMIT mappingAssigned(m_pInternal, pMapping, getEnabled());
    clearEdited();
    return true;
}

void QmlControllerDeviceProxy::clear() {
    m_enabled = std::nullopt;
    m_pMapping = nullptr;
    auto* pMapping = m_pInternal->getMapping().get();
    if (pMapping) {
        for (auto* mappingProxy : std::as_const(m_mappings)) {
            if (mappingProxy->definition().getPath() == pMapping->getPath()) {
                m_pMapping = mappingProxy;
                break;
            }
        }
    }
    clearEdited();
    Q_EMIT nameChanged();
    Q_EMIT visualUrlChanged();
    Q_EMIT mappingChanged();
    Q_EMIT enabledChanged();
}

std::shared_ptr<LegacyControllerMapping> QmlControllerDeviceProxy::instanceFor(
        const QString& mappingPath) const {
    return m_mappingInstance.value(mappingPath);
}

void QmlControllerDeviceProxy::setInstanceFor(
        const QString& mappingPath, std::shared_ptr<LegacyControllerMapping> pMapping) {
    m_mappingInstance.insert(mappingPath, pMapping);
}

QmlControllerManagerProxy::QmlControllerManagerProxy(
        std::shared_ptr<ControllerManager> pControllerManager, QObject* parent)
        : QObject(parent),
          m_pControllerManager(std::move(pControllerManager)) {
    connect(m_pControllerManager.get(),
            &ControllerManager::devicesChanged,
            this,
            &QmlControllerManagerProxy::refreshKnownDevices);
    refreshMappings();
    refreshKnownDevices();
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
    Q_UNUSED(pQmlEngine);
    Q_UNUSED(pJsEngine);
    auto* proxy = new QmlControllerManagerProxy(s_pControllerManager);
    return proxy;
}

void QmlControllerManagerProxy::refreshKnownDevices() {
    m_knownControllers = m_pControllerManager->getControllers();

    qDeleteAll(m_knownDevicesFound);
    m_knownDevicesFound.clear();
    qDeleteAll(m_unknownDevicesFound);
    m_unknownDevicesFound.clear();

    for (auto* pController : std::as_const(m_knownControllers)) {
        auto productInfo = pController->getProductInfo();
        QList<QmlControllerMappingProxy*> mappings;
        if (productInfo.has_value()) {
            mappings = m_knownDevices.value(productInfo.value()).values();
        }

        auto* pProxy = new QmlControllerDeviceProxy(
                pController, productInfo, mappings, this);
        connect(pProxy,
                &QmlControllerDeviceProxy::mappingAssigned,
                m_pControllerManager.get(),
                &ControllerManager::slotApplyMapping);
        connect(pProxy,
                &QmlControllerDeviceProxy::mappingCreated,
                this,
                &QmlControllerManagerProxy::loadNewMapping);
        connect(pProxy,
                &QmlControllerDeviceProxy::mappingUpdated,
                this,
                &QmlControllerManagerProxy::updateExistingMapping);
        connect(pController,
                &Controller::deviceLearned,
                pProxy,
                &QmlControllerDeviceProxy::deviceLearned);

        if (productInfo.has_value()) {
            m_knownDevicesFound.append(pProxy);
        } else {
            m_unknownDevicesFound.append(pProxy);
        }
    }

    Q_EMIT deviceListChanged();
}

void QmlControllerManagerProxy::refreshMappings() {
    m_knownDevices.clear();
    m_knownMappings.clear();

    loadMappingFromEnumerator(m_pControllerManager->getMainThreadUserMappingEnumerator());
    loadMappingFromEnumerator(m_pControllerManager->getMainThreadSystemMappingEnumerator());
}

void QmlControllerManagerProxy::loadNewMapping(
        ::mixxx::qml::QmlControllerDeviceProxy::Type type, const ::MappingInfo& mapping) {
    auto* pProxy = new QmlControllerMappingProxy(mapping, this);
    auto mappings = m_knownMappings.value(type);
    mappings.append(pProxy);
    m_knownMappings.insert(type, mappings);

    for (const auto& productInfo : mapping.getProducts()) {
        auto deviceMappings = m_knownDevices.value(productInfo);
        deviceMappings.insert(pProxy);
        m_knownDevices.insert(productInfo, deviceMappings);
    }

    refreshKnownDevices();
}

void QmlControllerManagerProxy::updateExistingMapping(
        ::mixxx::qml::QmlControllerMappingProxy* pMapping, const ::MappingInfo& mapping) {
    Q_EMIT pMapping->mappingErrored();
    refreshMappings();
    refreshKnownDevices();
}

void QmlControllerManagerProxy::loadMappingFromEnumerator(
        QSharedPointer<MappingInfoEnumerator> enumerator) {
    if (!enumerator) {
        return;
    }

    const QStringList extensions = {MIDI_MAPPING_EXTENSION, HID_MAPPING_EXTENSION, BULK_MAPPING_EXTENSION};
    for (const auto& extension : extensions) {
        auto mappings = enumerator->getMappingsByExtension(extension);
        for (const auto& mapping : std::as_const(mappings)) {
            auto* pProxy = new QmlControllerMappingProxy(mapping, this);
            auto type = (extension == MIDI_MAPPING_EXTENSION) ? QmlControllerDeviceProxy::Type::MIDI
                                                              : (extension == HID_MAPPING_EXTENSION) ? QmlControllerDeviceProxy::Type::HID
                                                                                                     : QmlControllerDeviceProxy::Type::BULK;
            auto typeMappings = m_knownMappings.value(type);
            typeMappings.append(pProxy);
            m_knownMappings.insert(type, typeMappings);

            for (const auto& productInfo : mapping.getProducts()) {
                auto deviceMappings = m_knownDevices.value(productInfo);
                deviceMappings.insert(pProxy);
                m_knownDevices.insert(productInfo, deviceMappings);
            }
        }
    }
}

} // namespace qml
} // namespace mixxx

#include "moc_qmlpreferencesproxy.cpp"
