#include "qml/qmlpreferencesproxy.h"

#include <qalgorithms.h>
#include <qlist.h>
#include <qqmlengine.h>

#include <QImage>
#include <QJSValue>
#include <QtDebug>
#include <memory>

#include "controllers/controller.h"
#include "controllers/controllermanager.h"
#include "controllers/legacycontrollermapping.h"
#include "library/library.h"
#include "moc_qmlpreferencesproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlControllerScreenElement::QmlControllerScreenElement(
        QObject* parent, const LegacyControllerMapping::ScreenInfo& screen)
        : QObject(parent),
          m_screenInfo(screen),
          m_lastFrameTimestamp(),
          m_averageFrameDuration(0) {
}

int QmlControllerScreenElement::fps() const {
    if (m_averageFrameDuration == 0) {
        return 0;
    }
    return static_cast<int>(1000000.0 / m_averageFrameDuration);
}

void QmlControllerScreenElement::updateFrame(
        const LegacyControllerMapping::ScreenInfo& screen, const QImage& frame) {
    auto now = mixxx::Time::now();
    if (m_lastFrameTimestamp != mixxx::Time::time_point()) {
        double duration = std::chrono::duration_cast<std::chrono::microseconds>(
                now - m_lastFrameTimestamp)
                                  .count();
        if (m_averageFrameDuration == 0) {
            m_averageFrameDuration = duration;
        } else {
            m_averageFrameDuration = m_averageFrameDuration * 0.9 + duration * 0.1;
        }
        Q_EMIT fpsChanged();
    }
    m_lastFrameTimestamp = now;

#ifndef Q_OS_ANDROID
    Q_EMIT videoFrameAvailable(QVideoFrame(frame));
#endif
}

void QmlControllerScreenElement::clear() {
    m_lastFrameTimestamp = mixxx::Time::time_point();
    m_averageFrameDuration = 0;
    Q_EMIT fpsChanged();
}

QmlControllerSettingItem::QmlControllerSettingItem(
        LegacyControllerSettingsLayoutItem* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
    connect(m_pInternal->setting(),
            &AbstractLegacyControllerSetting::valueChanged,
            this,
            &QmlControllerSettingElement::dirtyChanged);
}

QString QmlControllerSettingItem::label() const {
    return m_pInternal->label();
}

QJSValue QmlControllerSettingItem::value() const {
    return m_pInternal->setting()->value();
}

void QmlControllerSettingItem::setValue(const QJSValue& value) {
    m_pInternal->setting()->setValue(value);
}

QJSValue QmlControllerSettingItem::savedValue() const {
    return m_pInternal->setting()->savedValue();
}

QJSValue QmlControllerSettingItem::defaultValue() const {
    return m_pInternal->setting()->defaultValue();
}

QString QmlControllerSettingItem::type() const {
    return m_pInternal->setting()->variableName();
}

QmlControllerSettingContainer::QmlControllerSettingContainer(
        const LegacyControllerSettingsLayoutContainer* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
    for (auto* pChild : qAsConst(pInternal->children())) {
        if (auto* pItem = dynamic_cast<LegacyControllerSettingsLayoutItem*>(pChild)) {
            auto* pProxy = new QmlControllerSettingItem(pItem, this);
            m_children.append(pProxy);
            connect(pProxy,
                    &QmlControllerSettingElement::dirtyChanged,
                    this,
                    &QmlControllerSettingElement::dirtyChanged);
        } else if (auto* pContainer = dynamic_cast<LegacyControllerSettingsLayoutContainer*>(pChild)) {
            auto* pProxy = new QmlControllerSettingContainer(pContainer, this);
            m_children.append(pProxy);
            connect(pProxy,
                    &QmlControllerSettingElement::dirtyChanged,
                    this,
                    &QmlControllerSettingElement::dirtyChanged);
        }
    }
}

QQmlListProperty<QmlControllerSettingElement> QmlControllerSettingContainer::children() {
    return {this, &m_children};
}

LegacyControllerSettingsLayoutContainer::Disposition QmlControllerSettingContainer::disposition() const {
    return m_pInternal->disposition();
}

QmlControllerSettingGroup::QmlControllerSettingGroup(
        const LegacyControllerSettingsGroup* pInternal, QObject* parent)
        : QmlControllerSettingElement(parent),
          m_pInternal(pInternal) {
    for (auto* pChild : qAsConst(pInternal->children())) {
        if (auto* pItem = dynamic_cast<LegacyControllerSettingsLayoutItem*>(pChild)) {
            auto* pProxy = new QmlControllerSettingItem(pItem, this);
            m_children.append(pProxy);
            connect(pProxy,
                    &QmlControllerSettingElement::dirtyChanged,
                    this,
                    &QmlControllerSettingElement::dirtyChanged);
        } else if (auto* pContainer = dynamic_cast<LegacyControllerSettingsLayoutContainer*>(pChild)) {
            auto* pProxy = new QmlControllerSettingContainer(pContainer, this);
            m_children.append(pProxy);
            connect(pProxy,
                    &QmlControllerSettingElement::dirtyChanged,
                    this,
                    &QmlControllerSettingElement::dirtyChanged);
        }
    }
}

QString QmlControllerSettingGroup::label() const {
    return m_pInternal->label();
}

QQmlListProperty<QmlControllerSettingElement> QmlControllerSettingGroup::children() {
    return {this, &m_children};
}

LegacyControllerSettingsLayoutContainer::Disposition QmlControllerSettingGroup::disposition() const {
    return m_pInternal->disposition();
}

QmlControllerMappingProxy::QmlControllerMappingProxy(const MappingInfo& mapping, QObject* parent)
        : QObject(parent),
          m_mappingDefinition(mapping),
          m_hasSettings(std::nullopt),
          m_hasScreens(std::nullopt) {
}

QmlControllerSettingElement* QmlControllerMappingProxy::loadSettings(
        const QmlConfigProxy* pConfig, QmlControllerDeviceProxy* pController) {
    auto pMapping = pController->instanceFor(m_mappingDefinition.filePath());
    if (!pMapping) {
        return nullptr;
    }
    auto* pLayout = pMapping->settingsLayout();
    if (!pLayout) {
        return nullptr;
    }
    return new QmlControllerSettingGroup(pLayout, this);
}

QList<QmlControllerScreenElement*> QmlControllerMappingProxy::loadScreens(
        const QmlConfigProxy* pConfig, QmlControllerDeviceProxy* pController) {
    auto pMapping = pController->instanceFor(m_mappingDefinition.filePath());
    if (!pMapping) {
        return {};
    }
    QList<QmlControllerScreenElement*> screens;
    for (const auto& screen : qAsConst(pMapping->screens())) {
        screens.append(new QmlControllerScreenElement(this, screen));
    }
    return screens;
}

void QmlControllerMappingProxy::resetSettings(QmlControllerDeviceProxy* pController) {
    auto pMapping = pController->instanceFor(m_mappingDefinition.filePath());
    if (pMapping) {
        pMapping->resetSettings();
    }
}

QString QmlControllerMappingProxy::getName() const {
    return m_mappingDefinition.name();
}

QString QmlControllerMappingProxy::getAuthor() const {
    fetchMappingDetails();
    return m_mappingDefinition.author();
}

QString QmlControllerMappingProxy::getDescription() const {
    fetchMappingDetails();
    return m_mappingDefinition.description();
}

QUrl QmlControllerMappingProxy::getForumLink() const {
    fetchMappingDetails();
    return m_mappingDefinition.forumLink();
}

QUrl QmlControllerMappingProxy::getWikiLink() const {
    fetchMappingDetails();
    return m_mappingDefinition.wikiLink();
}

bool QmlControllerMappingProxy::hasSettings() const {
    if (!m_hasSettings.has_value()) {
        fetchMappingDetails();
    }
    return m_hasSettings.value_or(false);
}

bool QmlControllerMappingProxy::hasScreens() const {
    if (!m_hasScreens.has_value()) {
        fetchMappingDetails();
    }
    return m_hasScreens.value_or(false);
}

bool QmlControllerMappingProxy::isUserMapping(const QmlConfigProxy* pConfig) const {
    return m_mappingDefinition.filePath().startsWith(QmlLibraryProxy::get()->userMappingsPath());
}

void QmlControllerMappingProxy::fetchMappingDetails() const {
    auto pMapping = std::make_shared<LegacyControllerMapping>();
    if (pMapping->load(m_mappingDefinition.filePath())) {
        m_mappingDefinition.setAuthor(pMapping->author());
        m_mappingDefinition.setDescription(pMapping->description());
        m_mappingDefinition.setForumLink(pMapping->forumLink());
        m_mappingDefinition.setWikiLink(pMapping->wikiLink());
        m_hasSettings = pMapping->hasSettings();
        m_hasScreens = pMapping->hasScreens();
    } else {
        Q_EMIT mappingErrored();
    }
}

QmlControllerDeviceProxy::QmlControllerDeviceProxy(::Controller* pInternal,
        const std::optional<::ProductInfo>& productInfo,
        const QList<::mixxx::qml::QmlControllerMappingProxy*>& mappings,
        QObject* parent)
        : QObject(parent),
          m_edited(false),
          m_pInternal(pInternal),
          m_productInfo(productInfo),
          m_mappings(mappings),
          m_pMapping(nullptr),
          m_enabled(std::nullopt) {
    for (auto* pMapping : mappings) {
        if (pMapping->definition().filePath() == pInternal->mappingPath()) {
            m_pMapping = pMapping;
            break;
        }
    }
}

QmlControllerDeviceProxy::Type QmlControllerDeviceProxy::getType() const {
    switch (m_pInternal->getDataRepresentationProtocol()) {
    case ::Controller::DataRepresentationProtocol::MIDI:
        return Type::MIDI;
    case ::Controller::DataRepresentationProtocol::HID:
        return Type::HID;
    default:
        return Type::BULK;
    }
}

QString QmlControllerDeviceProxy::getName() const {
    return m_pInternal->getName();
}

QmlControllerMappingProxy* QmlControllerDeviceProxy::getMapping() const {
    return m_pMapping;
}

void QmlControllerDeviceProxy::setMapping(QmlControllerMappingProxy* mapping) {
    if (m_pMapping != mapping) {
        m_pMapping = mapping;
        setEdited();
        Q_EMIT mappingChanged();
    }
}

bool QmlControllerDeviceProxy::getEnabled() const {
    return m_enabled.value_or(m_pInternal->isOpen());
}

void QmlControllerDeviceProxy::setEnabled(bool state) {
    if (getEnabled() != state) {
        m_enabled = state;
        setEdited();
        Q_EMIT enabledChanged();
    }
}

QString QmlControllerDeviceProxy::vendor() const {
    return m_productInfo ? m_productInfo->vendor : QString();
}

QString QmlControllerDeviceProxy::product() const {
    return m_productInfo ? m_productInfo->name : QString();
}

QString QmlControllerDeviceProxy::serialNumber() const {
    return m_pInternal->getSerialNumber();
}

bool QmlControllerDeviceProxy::save(const QmlConfigProxy* pConfig) {
    if (!m_edited) {
        return true;
    }
    bool enabled = getEnabled();
    auto pMapping = m_pMapping ? instanceFor(m_pMapping->definition().filePath()) : nullptr;
    Q_EMIT mappingAssigned(m_pInternal, pMapping.get(), enabled);
    clearEdited();
    return true;
}

void QmlControllerDeviceProxy::clear() {
    m_pMapping = nullptr;
    for (auto* pMapping : qAsConst(m_mappings)) {
        if (pMapping->definition().filePath() == m_pInternal->mappingPath()) {
            m_pMapping = pMapping;
            break;
        }
    }
    m_enabled = std::nullopt;
    clearEdited();
    Q_EMIT mappingChanged();
    Q_EMIT enabledChanged();
}

std::shared_ptr<LegacyControllerMapping> QmlControllerDeviceProxy::instanceFor(const QString& mappingPath) const {
    if (m_mappingInstance.contains(mappingPath)) {
        return m_mappingInstance.value(mappingPath);
    }
    auto pMapping = std::make_shared<LegacyControllerMapping>();
    if (pMapping->load(mappingPath)) {
        const_cast<QmlControllerDeviceProxy*>(this)->m_mappingInstance.insert(mappingPath, pMapping);
        return pMapping;
    }
    return nullptr;
}

void QmlControllerDeviceProxy::setInstanceFor(const QString& mappingPath, std::shared_ptr<LegacyControllerMapping> pMapping) {
    m_mappingInstance.insert(mappingPath, pMapping);
}

QmlControllerManagerProxy::QmlControllerManagerProxy(std::shared_ptr<::ControllerManager> pControllerManager, QObject* parent)
        : QObject(parent),
          m_pControllerManager(std::move(pControllerManager)) {
}

QQmlListProperty<QmlControllerDeviceProxy> QmlControllerManagerProxy::knownDevices() {
    return {this, &m_knownDevicesFound};
}

QQmlListProperty<QmlControllerDeviceProxy> QmlControllerManagerProxy::unknownDevices() {
    return {this, &m_unknownDevicesFound};
}

bool QmlControllerManagerProxy::isControllerScreenDebug() const {
    return s_controllerPreviewScreens;
}

std::shared_ptr<::ControllerManager> QmlControllerManagerProxy::internal() const {
    return m_pControllerManager;
}

// static
QmlControllerManagerProxy* QmlControllerManagerProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    if (!s_pControllerManager) {
        return nullptr;
    }
    auto* pProxy = new QmlControllerManagerProxy(s_pControllerManager, pQmlEngine);
    pProxy->refreshMappings();
    pProxy->refreshKnownDevices();
    return pProxy;
}

void QmlControllerManagerProxy::refreshKnownDevices() {
    qDeleteAll(m_knownDevicesFound);
    m_knownDevicesFound.clear();
    qDeleteAll(m_unknownDevicesFound);
    m_unknownDevicesFound.clear();

    m_knownControllers = m_pControllerManager->getControllers();
    for (auto* pInternal : qAsConst(m_knownControllers)) {
        auto type = QmlControllerDeviceProxy::Type::BULK;
        switch (pInternal->getDataRepresentationProtocol()) {
        case ::Controller::DataRepresentationProtocol::MIDI:
            type = QmlControllerDeviceProxy::Type::MIDI;
            break;
        case ::Controller::DataRepresentationProtocol::HID:
            type = QmlControllerDeviceProxy::Type::HID;
            break;
        default:
            break;
        }

        auto* pProxy = new QmlControllerDeviceProxy(pInternal,
                m_pControllerManager->getProductInfo(pInternal),
                m_knownMappings.value(type),
                this);
        if (pProxy->getMapping()) {
            m_knownDevicesFound.append(pProxy);
        } else {
            m_unknownDevicesFound.append(pProxy);
        }
    }
    Q_EMIT deviceListChanged();
}

void QmlControllerManagerProxy::refreshMappings() {
    m_knownMappings.clear();
    loadMappingFromEnumerator(m_pControllerManager->getMidiEnumerator());
    loadMappingFromEnumerator(m_pControllerManager->getHidEnumerator());
    loadMappingFromEnumerator(m_pControllerManager->getBulkEnumerator());
}

void QmlControllerManagerProxy::loadNewMapping(::mixxx::qml::QmlControllerDeviceProxy::Type type, const ::MappingInfo& mapping) {
    auto* pMappingProxy = new QmlControllerMappingProxy(mapping, this);
    m_knownMappings[type].append(pMappingProxy);
    refreshKnownDevices();
}

void QmlControllerManagerProxy::updateExistingMapping(::mixxx::qml::QmlControllerMappingProxy* pMapping, const ::MappingInfo& mapping) {
    // In current implementation, we just refresh everything as the underlying
    // mapping info might have changed significantly.
    refreshMappings();
    refreshKnownDevices();
}

void QmlControllerManagerProxy::loadMappingFromEnumerator(QSharedPointer<MappingInfoEnumerator> enumerator) {
    auto type = QmlControllerDeviceProxy::Type::BULK;
    if (enumerator == m_pControllerManager->getMidiEnumerator()) {
        type = QmlControllerDeviceProxy::Type::MIDI;
    } else if (enumerator == m_pControllerManager->getHidEnumerator()) {
        type = QmlControllerDeviceProxy::Type::HID;
    }

    QList<QmlControllerMappingProxy*> mappings;
    for (const auto& mapping : qAsConst(enumerator->getMappings())) {
        mappings.append(new QmlControllerMappingProxy(mapping, this));
    }
    m_knownMappings.insert(type, mappings);
}

} // namespace qml
} // namespace mixxx
