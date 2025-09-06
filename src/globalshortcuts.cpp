/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "globalshortcuts.h"
// config
#include "config-kwin.h"
// kwin
#include "configurablegesture.h"
#include "effect/globals.h"
#include "gestures.h"
#include "main.h"
#include "utils/common.h"
// KDE
#if KWIN_BUILD_GLOBALSHORTCUTS
#include "kglobalaccelinterface.h"
#include <kglobalaccel_interface.h>
#include <kglobalacceld.h>
#include <kglobalshortcuttrigger.h>
#endif
// Qt
#include <QAction>
// system
#include <signal.h>
#include <variant>

namespace KWin
{

GlobalShortcut::GlobalShortcut(Shortcut &&sc, QAction *action)
    : m_shortcut(sc)
    , m_action(action)
{
    if (auto swipeGesture = std::get_if<RealtimeFeedbackSwipeShortcut>(&sc)) {
        m_swipeGesture = std::make_unique<SwipeGesture>(swipeGesture->fingerCount);
        m_swipeGesture->setDirection(swipeGesture->direction);
        QObject::connect(m_swipeGesture.get(), &SwipeGesture::triggered, m_action, &QAction::trigger, Qt::QueuedConnection);
        if (swipeGesture->progressCallback) {
            QObject::connect(m_swipeGesture.get(), &SwipeGesture::progress, m_swipeGesture.get(), swipeGesture->progressCallback);
        }
        if (swipeGesture->cancelledCallback) {
            QObject::connect(m_swipeGesture.get(), &SwipeGesture::cancelled, m_swipeGesture.get(), swipeGesture->cancelledCallback);
        } else {
            QObject::connect(m_swipeGesture.get(), &SwipeGesture::cancelled, m_action, &QAction::trigger, Qt::QueuedConnection);
        }
    } else if (auto pinchGesture = std::get_if<RealtimeFeedbackPinchShortcut>(&sc)) {
        m_pinchGesture = std::make_unique<PinchGesture>(pinchGesture->fingerCount);
        m_pinchGesture->setDirection(pinchGesture->direction);
        QObject::connect(m_pinchGesture.get(), &PinchGesture::triggered, m_action, &QAction::trigger, Qt::QueuedConnection);
        if (pinchGesture->scaleCallback) {
            QObject::connect(m_pinchGesture.get(), &PinchGesture::progress, m_pinchGesture.get(), pinchGesture->scaleCallback);
        }
        if (pinchGesture->cancelledCallback) {
            QObject::connect(m_pinchGesture.get(), &PinchGesture::cancelled, m_pinchGesture.get(), pinchGesture->cancelledCallback);
        } else {
            QObject::connect(m_pinchGesture.get(), &PinchGesture::cancelled, m_action, &QAction::trigger, Qt::QueuedConnection);
        }
    }
}

GlobalShortcut::~GlobalShortcut()
{
}

QAction *GlobalShortcut::action() const
{
    return m_action;
}

void GlobalShortcut::invoke() const
{
    QMetaObject::invokeMethod(m_action, &QAction::trigger, Qt::QueuedConnection);
}

const Shortcut &GlobalShortcut::shortcut() const
{
    return m_shortcut;
}

SwipeGesture *GlobalShortcut::swipeGesture() const
{
    return m_swipeGesture.get();
}

PinchGesture *GlobalShortcut::pinchGesture() const
{
    return m_pinchGesture.get();
}

GlobalShortcutsManager::GlobalShortcutsManager(QObject *parent)
    : QObject(parent)
    , m_touchpadGestureRecognizer(new GestureRecognizer(this))
    , m_touchscreenGestureRecognizer(new GestureRecognizer(this))
{
}

GlobalShortcutsManager::~GlobalShortcutsManager()
{
}

void GlobalShortcutsManager::init()
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    auto iface = std::make_unique<KGlobalAccelImpl>();
    m_kglobalAccelInterface = iface.get();
    m_kglobalAccel = std::make_unique<KGlobalAccelD>(std::move(iface));
    if (!m_kglobalAccel->init()) {
        qCDebug(KWIN_CORE) << "Init of kglobalaccel failed";
        m_kglobalAccel.reset();
    } else {
        // clang-format off
        connect(m_kglobalAccelInterface, // actually a KGlobalAccelImpl from kglobalaccel_plugin.h
                SIGNAL(triggerActive(QString,bool,QString,QString,QString,QString)),
                this,
                SLOT(onKGlobalShortcutTriggerActive(QString,bool,QString,QString,QString,QString)),
                Qt::QueuedConnection); // allow time for registerGesture() before activating the gesture
        // clang-format on

        initDefaultGestures();
    }
#endif
}

void GlobalShortcutsManager::objectDeleted(QObject *object)
{
    auto it = m_shortcuts.begin();
    while (it != m_shortcuts.end()) {
        if (it->action() == object) {
            it = m_shortcuts.erase(it);
        } else {
            ++it;
        }
    }
}

bool GlobalShortcutsManager::add(GlobalShortcut sc, DeviceType device)
{
    const auto &recognizer = device == DeviceType::Touchpad ? m_touchpadGestureRecognizer : m_touchscreenGestureRecognizer;
    if (std::holds_alternative<RealtimeFeedbackSwipeShortcut>(sc.shortcut())) {
        recognizer->registerSwipeGesture(sc.swipeGesture());
    } else if (std::holds_alternative<RealtimeFeedbackPinchShortcut>(sc.shortcut())) {
        recognizer->registerPinchGesture(sc.pinchGesture());
    }
    connect(sc.action(), &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
    m_shortcuts.push_back(std::move(sc));
    return true;
}

void GlobalShortcutsManager::registerPointerShortcut(QAction *action, Qt::KeyboardModifiers modifiers, Qt::MouseButtons pointerButtons)
{
    add(GlobalShortcut(PointerButtonShortcut{modifiers, pointerButtons}, action));
}

void GlobalShortcutsManager::registerAxisShortcut(QAction *action, Qt::KeyboardModifiers modifiers, PointerAxisDirection axis)
{
    add(GlobalShortcut(PointerAxisShortcut{modifiers, axis}, action));
}

void GlobalShortcutsManager::registerTouchpadSwipe(SwipeDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback, std::function<void()> cancelledCallback)
{
    add(GlobalShortcut(RealtimeFeedbackSwipeShortcut{DeviceType::Touchpad, direction, progressCallback, cancelledCallback, fingerCount}, action), DeviceType::Touchpad);
}

void GlobalShortcutsManager::registerTouchpadSwipe(SwipeGesture *swipeGesture)
{
    m_touchpadGestureRecognizer->registerSwipeGesture(swipeGesture);
}

void GlobalShortcutsManager::registerTouchpadPinch(PinchDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback, std::function<void()> cancelledCallback)
{
    add(GlobalShortcut(RealtimeFeedbackPinchShortcut{direction, progressCallback, cancelledCallback, fingerCount}, action), DeviceType::Touchpad);
}

void GlobalShortcutsManager::registerTouchpadPinch(PinchGesture *pinchGesture)
{
    m_touchpadGestureRecognizer->registerPinchGesture(pinchGesture);
}

void GlobalShortcutsManager::registerTouchscreenSwipe(SwipeDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback, std::function<void()> cancelledCallback)
{
    add(GlobalShortcut(RealtimeFeedbackSwipeShortcut{DeviceType::Touchscreen, direction, progressCallback, cancelledCallback, fingerCount}, action), DeviceType::Touchscreen);
}

void GlobalShortcutsManager::registerTouchscreenSwipe(SwipeGesture *swipeGesture)
{
    m_touchscreenGestureRecognizer->registerSwipeGesture(swipeGesture);
}

void GlobalShortcutsManager::forceRegisterTouchscreenSwipe(SwipeDirection direction, uint32_t fingerCount, QAction *action, std::function<void(qreal)> progressCallback, std::function<void()> cancelledCallback)
{
    GlobalShortcut shortcut{RealtimeFeedbackSwipeShortcut{DeviceType::Touchscreen, direction, progressCallback, cancelledCallback, fingerCount}, action};
    const auto it = std::find_if(m_shortcuts.begin(), m_shortcuts.end(), [&shortcut](const auto &s) {
        return shortcut.shortcut() == s.shortcut();
    });
    if (it != m_shortcuts.end()) {
        m_shortcuts.erase(it);
    }
    m_touchscreenGestureRecognizer->registerSwipeGesture(shortcut.swipeGesture());
    connect(shortcut.action(), &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
    m_shortcuts.push_back(std::move(shortcut));
}

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, int keyQt, KeyboardKeyState state)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    if (m_kglobalAccelInterface) {
        bool handled = false;
        QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                  "checkKeyPressed",
                                  Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, handled),
                                  Q_ARG(int, int(mods) | keyQt),
                                  Q_ARG(KeyboardKeyState, state));
        return handled;
    }
#endif
    return false;
}

template<typename ShortcutKind, typename... Args>
GlobalShortcut *match(QList<GlobalShortcut> &shortcuts, Args... args)
{
    for (auto &sc : shortcuts) {
        if (std::holds_alternative<ShortcutKind>(sc.shortcut())) {
            if (std::get<ShortcutKind>(sc.shortcut()) == ShortcutKind{args...}) {
                return &sc;
            }
        }
    }
    return nullptr;
}

// TODO(C++20): use ranges for a nicer way of filtering by shortcut type
bool GlobalShortcutsManager::processPointerPressed(Qt::KeyboardModifiers mods, Qt::MouseButtons pointerButtons)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    // currently only used to better support modifier only shortcuts
    // modifier-only shortcuts are not triggered if a pointer button is pressed
    if (m_kglobalAccelInterface) {
        QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                  "checkPointerPressed",
                                  Qt::DirectConnection,
                                  Q_ARG(Qt::MouseButtons, pointerButtons));
    }
#endif
    GlobalShortcut *shortcut = match<PointerButtonShortcut>(m_shortcuts, mods, pointerButtons);
    if (shortcut) {
        shortcut->invoke();
    }
    return shortcut != nullptr;
}

bool GlobalShortcutsManager::processAxis(Qt::KeyboardModifiers mods, PointerAxisDirection axis, qreal delta)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    // currently only used to better support modifier only shortcuts
    // modifier-only shortcuts are not triggered if a pointer axis is used
    if (m_kglobalAccelInterface) {
        QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                  "checkAxisTriggered",
                                  Qt::DirectConnection,
                                  Q_ARG(int, axis));
    }
#endif
    GlobalShortcut *shortcut = match<PointerAxisShortcut>(m_shortcuts, mods, axis);
    if (shortcut && std::abs(delta) >= 1.0f) {
        shortcut->invoke();
    }
    return shortcut != nullptr;
}

void GlobalShortcutsManager::processSwipeStart(DeviceType device, uint fingerCount)
{
    if (device == DeviceType::Touchpad) {
        m_touchpadGestureRecognizer->startSwipeGesture(fingerCount);
    } else {
        m_touchscreenGestureRecognizer->startSwipeGesture(fingerCount);
    }
}

void GlobalShortcutsManager::processSwipeUpdate(DeviceType device, const QPointF &delta)
{
    if (device == DeviceType::Touchpad) {
        m_touchpadGestureRecognizer->updateSwipeGesture(delta);
    } else {
        m_touchscreenGestureRecognizer->updateSwipeGesture(delta);
    }
}

void GlobalShortcutsManager::processSwipeCancel(DeviceType device)
{
    if (device == DeviceType::Touchpad) {
        m_touchpadGestureRecognizer->cancelSwipeGesture();
    } else {
        m_touchscreenGestureRecognizer->cancelSwipeGesture();
    }
}

void GlobalShortcutsManager::processSwipeEnd(DeviceType device)
{
    if (device == DeviceType::Touchpad) {
        m_touchpadGestureRecognizer->endSwipeGesture();
    } else {
        m_touchscreenGestureRecognizer->endSwipeGesture();
    }
    // TODO: cancel on Wayland Seat if one triggered
}

void GlobalShortcutsManager::processPinchStart(uint fingerCount)
{
    m_touchpadGestureRecognizer->startPinchGesture(fingerCount);
}

void GlobalShortcutsManager::processPinchUpdate(qreal scale, qreal angleDelta, const QPointF &delta)
{
    m_touchpadGestureRecognizer->updatePinchGesture(scale, angleDelta, delta);
}

void GlobalShortcutsManager::processPinchCancel()
{
    m_touchpadGestureRecognizer->cancelPinchGesture();
}

void GlobalShortcutsManager::processPinchEnd()
{
    m_touchpadGestureRecognizer->endPinchGesture();
}

void GlobalShortcutsManager::cancelModiferOnlySequence()
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    if (m_kglobalAccelInterface) {
        QMetaObject::invokeMethod(m_kglobalAccelInterface, "cancelModiferOnlySequence");
    }
#endif
}

static QString getComponentName(QAction *shortcutAction)
{
    auto componentNameProp = shortcutAction->property("componentName");
    return componentNameProp.isValid() ? componentNameProp.toString() : QCoreApplication::applicationName();
}

static QString makeShortcutActionKey(const QString &componentName, const QString &actionId)
{
    return componentName % ":" % actionId;
}

ConfigurableGesture *GlobalShortcutsManager::registerGesture(QAction *shortcutAction)
{
    auto gesture = new ConfigurableGesture(this, shortcutAction);
#if KWIN_BUILD_GLOBALSHORTCUTS
    gesture->setObjectName(makeShortcutActionKey(getComponentName(shortcutAction), shortcutAction->objectName()));

    ConfigurableGesture *previousGesture = m_perActionGestures.value(gesture->objectName(), nullptr);
    m_perActionGestures[gesture->objectName()] = gesture;

    // activate gestures that KGlobalAccel already wanted active, but are only now registered here by KWin
    for (auto [triggerId, active] : m_kglobalAccelActiveTriggers.asKeyValueRange()) {
        if (active.shortcutActionKey == gesture->objectName()) {
            activateGesture(active, gesture, triggerId);
        }
    }

    if (previousGesture && previousGesture->isAutoCreated()) {
        m_autoGestures.erase(previousGesture->objectName());
    }
#endif
    return gesture;
}

void GlobalShortcutsManager::unregisterGesture(ConfigurableGesture *gesture)
{
    if (gesture == nullptr) {
        return;
    }

#if KWIN_BUILD_GLOBALSHORTCUTS
    ConfigurableGesture *replacementGesture = nullptr;

    auto it = m_perActionGestures.find(gesture->objectName());
    if (it != m_perActionGestures.end() && it.value() == gesture) {
        if (gesture->triggerActionCount() > 0 || gesture->isAutoCreated()) {
            m_perActionGestures.erase(it);
        } else {
            // we unregistered a manually registered ConfigurableGesture, replace it with the
            // next one in line (or create an auto gesture if the list is empty)
            replacementGesture = ensureAutoGesture(gesture->objectName());
            it.value() = replacementGesture;
        }
    }

    for (auto [triggerId, active] : m_kglobalAccelActiveTriggers.asKeyValueRange()) {
        if (active.configurableGesture == gesture) {
            if (replacementGesture) {
                activateGesture(active, replacementGesture, triggerId);
            } else {
                deactivateGesture(active);
            }
        }
    }
#endif
}

#if KWIN_BUILD_GLOBALSHORTCUTS
ConfigurableGesture *GlobalShortcutsManager::ensureAutoGesture(const QString &shortcutActionKey)
{
    auto &autoGesture = m_autoGestures[shortcutActionKey];
    if (!autoGesture) {
        autoGesture = std::make_unique<ConfigurableGesture>(this);
        autoGesture->setObjectName(shortcutActionKey);
        autoGesture->setAutoCreated();
    }
    return autoGesture.get();
}

void GlobalShortcutsManager::onKGlobalShortcutTriggerActive(const QString &triggerId,
                                                            bool active,
                                                            const QString &componentName,
                                                            const QString &actionId,
                                                            const QString &componentFriendlyName,
                                                            const QString &actionFriendlyName)
{
    const QString shortcutActionKey = makeShortcutActionKey(componentName, actionId);

    if (active) {
        // Ensure that m_perActionGestures[shortcutActionKey] exists while any of the action's
        // triggers is active. The actual ConfigurableGesture corresponding to this action is only
        // created on demand, if no other ConfigurableGesture has been registered by KWin code.
        ConfigurableGesture *actionGesture = m_perActionGestures.value(shortcutActionKey, nullptr);
        if (!actionGesture) {
            actionGesture = ensureAutoGesture(shortcutActionKey);
            m_perActionGestures[shortcutActionKey] = actionGesture;
        }

        ActiveTriggerInfo &active = m_kglobalAccelActiveTriggers[triggerId];
        active.shortcutActionKey = shortcutActionKey;

        activateGesture(active, actionGesture, triggerId);
    }

    if (!active) {
        if (auto it = m_kglobalAccelActiveTriggers.find(triggerId); it != m_kglobalAccelActiveTriggers.end()) {
            deactivateGesture(it.value());
            m_kglobalAccelActiveTriggers.erase(it);
        }
        if (auto it = m_perActionGestures.find(shortcutActionKey); it != m_perActionGestures.end()) {
            if (!it.value() || it.value()->triggerActionCount() == 0) {
                m_perActionGestures.erase(it);
                m_autoGestures.erase(shortcutActionKey);
            }
        }
    }
}

static std::optional<SwipeDirection> toKWinSwipeDirection(KGlobalShortcutTriggerTypes::SwipeDirection direction)
{
    switch (direction) {
    case KGlobalShortcutTriggerTypes::SwipeDirection::Left:
        return ::KWin::SwipeDirection::Left;
    case KGlobalShortcutTriggerTypes::SwipeDirection::Up:
        return ::KWin::SwipeDirection::Up;
    case KGlobalShortcutTriggerTypes::SwipeDirection::Right:
        return ::KWin::SwipeDirection::Right;
    case KGlobalShortcutTriggerTypes::SwipeDirection::Down:
        return ::KWin::SwipeDirection::Down;
    default:
        return std::nullopt;
    }
}

static std::optional<PinchDirection> toKWinPinchDirection(KGlobalShortcutTriggerTypes::PinchDirection direction)
{
    switch (direction) {
    case KGlobalShortcutTriggerTypes::PinchDirection::Expanding:
        return ::KWin::PinchDirection::Expanding;
    case KGlobalShortcutTriggerTypes::PinchDirection::Contracting:
        return ::KWin::PinchDirection::Contracting;
    default:
        return std::nullopt;
    }
}

bool GlobalShortcutsManager::activateGesture(ActiveTriggerInfo &active, ConfigurableGesture *gesture, const QString &triggerId)
{
    deactivateGesture(active); // don't allow duplicate gestures with the same trigger parameters

    if (!gesture) {
        return false;
    }

    auto assignNewTriggerAction = [this, &active, gesture, &triggerId]() -> QAction * {
        active.configurableGesture = gesture;
        delete active.gestureAction; // should be nullptr already, but make sure we won't leak
        active.gestureAction = gesture->makeAutoCountingTriggerAction();

        if (active.configurableGesture->isAutoCreated()) {
            auto sendTriggered = [interface = m_kglobalAccelInterface, triggerId]() {
                if (interface) {
                    QMetaObject::invokeMethod(interface,
                                              "checkTriggerEvent",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, triggerId),
                                              Q_ARG(int, static_cast<int>(ShortcutTriggerEvent::Triggered)));
                }
            };
            connect(active.configurableGesture, &ConfigurableGesture::triggered, active.gestureAction, sendTriggered);
        }
        return active.gestureAction;
    };

    auto emitProgress = [gesture](qreal progress) {
        Q_EMIT gesture->progress(progress);
    };
    auto emitCancelled = [gesture]() {
        Q_EMIT gesture->cancelled();
    };

    auto trigger = KGlobalShortcutTrigger::fromString(triggerId);

    if (const auto *g = trigger.asTouchpadSwipeGesture()) {
        if (const auto direction = toKWinSwipeDirection(g->direction); direction.has_value()) {
            registerTouchpadSwipe(*direction, g->fingerCount, assignNewTriggerAction(), emitProgress, emitCancelled);
            return true;
        }
    } else if (const auto *g = trigger.asTouchscreenSwipeGesture()) {
        if (const auto direction = toKWinSwipeDirection(g->direction); direction.has_value()) {
            registerTouchscreenSwipe(*direction, g->fingerCount, assignNewTriggerAction(), emitProgress, emitCancelled);
            return true;
        }
    } else if (const auto *g = trigger.asTouchpadPinchGesture()) {
        if (const auto direction = toKWinPinchDirection(g->direction); direction.has_value()) {
            registerTouchpadPinch(*direction, g->fingerCount, assignNewTriggerAction(), emitProgress, emitCancelled);
            return true;
        }
    } /* else if (const auto *g = trigger.asTouchscreenPinchGesture()) { // pinch not currently implemented for touchscreens?
        if (const auto direction = toKWinPinchDirection(g->direction); direction.has_value()) {
            registerTouchscreenPinch(*direction, g->fingerCount, assignNewTriggerAction(), emitProgress, emitCancelled);
            return true;
        }
    }*/
    // TODO: support more gesture types (incl. hold gestures, screen edge gestures, line shape gestures)

    return false;
}

void GlobalShortcutsManager::deactivateGesture(ActiveTriggerInfo &active)
{
    active.configurableGesture = nullptr;

    // we'd use unique_ptr<QAction> and .reset(), but then we need m_kglobalAccelActiveTriggers
    // to be an unordered_map for moveability, and unordered_map doesn't natively support pair keys
    delete active.gestureAction;
    active.gestureAction = nullptr;
}

void GlobalShortcutsManager::initDefaultGestures()
{
    // The initial idea was to add default gesture assignments to registerGesture() as a parameter,
    // but KGlobalAccelD support is optional and so a list of triggers shouldn't be exposed in the
    // the GlobalShortcutsManager API. Instead, we just define all default gestures here centrally.

    using SwipeDir = KGlobalShortcutTriggerTypes::SwipeDirection;
    using PinchDir = KGlobalShortcutTriggerTypes::PinchDirection;

    auto makeSwipes = [](int fingers, SwipeDir direction) -> QSet<KGlobalShortcutTrigger> {
        return {
            {KGlobalShortcutTriggerTypes::TouchpadSwipeGesture{.fingerCount = fingers, .direction = direction}},
            {KGlobalShortcutTriggerTypes::TouchscreenSwipeGesture{.fingerCount = fingers, .direction = direction}},
        };
    };
    auto makePinches = [](int fingers, PinchDir direction) -> QSet<KGlobalShortcutTrigger> {
        return {
            {KGlobalShortcutTriggerTypes::TouchpadPinchGesture{.fingerCount = fingers, .direction = direction}},
            {KGlobalShortcutTriggerTypes::TouchscreenPinchGesture{.fingerCount = fingers, .direction = direction}},
        };
    };

    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "Cycle Overview", makeSwipes(3, SwipeDir::Up));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "Cycle Overview Opposite", makeSwipes(3, SwipeDir::Down));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "Switch One Desktop Down", makeSwipes(4, SwipeDir::Up));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "Switch One Desktop Up", makeSwipes(4, SwipeDir::Down));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "Switch One Desktop to the Left", makeSwipes(4, SwipeDir::Right));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "Switch One Desktop to the Right", makeSwipes(4, SwipeDir::Left));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "view_zoom_in", makePinches(3, PinchDir::Expanding));
    m_kglobalAccel->setDefaultShortcutTriggers("kwin", "view_zoom_out", makePinches(3, PinchDir::Contracting));
}
#endif // KWIN_BUILD_GLOBALSHORTCUTS

} // namespace

#include "moc_globalshortcuts.cpp"
