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
#include "effect/globals.h"
#include "gestures.h"
#include "main.h"
#include "utils/common.h"
// KDE
#if KWIN_BUILD_GLOBALSHORTCUTS
#include <kglobalaccel_interface.h>
#include <kglobalacceld.h>
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
        m_swipeGesture = std::make_unique<SwipeGesture>();
        m_swipeGesture->setDirection(swipeGesture->direction);
        m_swipeGesture->setMinimumDelta(QPointF(200, 200));
        m_swipeGesture->setMaximumFingerCount(swipeGesture->fingerCount);
        m_swipeGesture->setMinimumFingerCount(swipeGesture->fingerCount);
        QObject::connect(m_swipeGesture.get(), &SwipeGesture::triggered, m_action, &QAction::trigger, Qt::QueuedConnection);
        QObject::connect(m_swipeGesture.get(), &SwipeGesture::cancelled, m_action, &QAction::trigger, Qt::QueuedConnection);
        if (swipeGesture->progressCallback) {
            QObject::connect(m_swipeGesture.get(), &SwipeGesture::progress, swipeGesture->progressCallback);
        }
    } else if (auto pinchGesture = std::get_if<RealtimeFeedbackPinchShortcut>(&sc)) {
        m_pinchGesture = std::make_unique<PinchGesture>();
        m_pinchGesture->setDirection(pinchGesture->direction);
        m_pinchGesture->setMaximumFingerCount(pinchGesture->fingerCount);
        m_pinchGesture->setMinimumFingerCount(pinchGesture->fingerCount);
        QObject::connect(m_pinchGesture.get(), &PinchGesture::triggered, m_action, &QAction::trigger, Qt::QueuedConnection);
        QObject::connect(m_pinchGesture.get(), &PinchGesture::cancelled, m_action, &QAction::trigger, Qt::QueuedConnection);
        if (pinchGesture->scaleCallback) {
            QObject::connect(m_pinchGesture.get(), &PinchGesture::progress, pinchGesture->scaleCallback);
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
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        qputenv("KGLOBALACCELD_PLATFORM", QByteArrayLiteral("org.kde.kwin"));
        m_kglobalAccel = std::make_unique<KGlobalAccelD>();
        if (!m_kglobalAccel->init()) {
            qCDebug(KWIN_CORE) << "Init of kglobalaccel failed";
            m_kglobalAccel.reset();
        } else {
            qCDebug(KWIN_CORE) << "KGlobalAcceld inited";
        }
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

void GlobalShortcutsManager::registerTouchGesture(const QString &name, const QString &description, QAction *action, std::function<void(qreal)> progressCallback)
{
    m_gestures.push_back(GestureDefinition{
        .name = name,
        .description = description,
        .action = action,
        .progressCallback = progressCallback,
    });
    reloadConfig();
}

void GlobalShortcutsManager::unregisterTouchGesture(const QString &name)
{
    std::erase_if(m_gestures, [name](const GestureDefinition &def) {
        return def.name == name;
    });
    reloadConfig();
}

void GlobalShortcutsManager::reloadConfig()
{
    // first, remove all previous gestures
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end();) {
        const auto shortcut = *it;
        if (std::holds_alternative<RealtimeFeedbackSwipeShortcut>(shortcut.shortcut())) {
            m_touchpadGestureRecognizer->unregisterSwipeGesture(shortcut.swipeGesture());
            m_touchscreenGestureRecognizer->unregisterSwipeGesture(shortcut.swipeGesture());
            it = m_shortcuts.erase(it);
        } else if (std::holds_alternative<RealtimeFeedbackPinchShortcut>(shortcut.shortcut())) {
            m_touchpadGestureRecognizer->unregisterPinchGesture(shortcut.pinchGesture());
            m_touchscreenGestureRecognizer->unregisterPinchGesture(shortcut.pinchGesture());
            it = m_shortcuts.erase(it);
        } else {
            it++;
        }
    }

    // now, reassign the ones we really want
    // there's no actual config yet, but this at least centralizes the defaults
    const auto findGesture = [this](const QString &name) {
        const auto it = std::ranges::find_if(m_gestures, [name](const GestureDefinition &gesture) {
            return gesture.name == name;
        });
        return it == m_gestures.end() ? nullptr : &*it;
    };
    if (auto g = findGesture("overview")) {
        add(GlobalShortcut(RealtimeFeedbackSwipeShortcut{DeviceType::Touchpad, SwipeDirection::Up, g->progressCallback, 4}, g->action), DeviceType::Touchpad);
        add(GlobalShortcut(RealtimeFeedbackSwipeShortcut{DeviceType::Touchpad, SwipeDirection::Up, g->progressCallback, 3}, g->action), DeviceType::Touchscreen);
    }
    if (auto g = findGesture("desktop grid")) {
        add(GlobalShortcut(RealtimeFeedbackSwipeShortcut{DeviceType::Touchpad, SwipeDirection::Down, g->progressCallback, 4}, g->action), DeviceType::Touchpad);
        add(GlobalShortcut(RealtimeFeedbackSwipeShortcut{DeviceType::Touchpad, SwipeDirection::Down, g->progressCallback, 3}, g->action), DeviceType::Touchscreen);
    }
}

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, int keyQt)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    if (m_kglobalAccelInterface) {
        auto check = [this](Qt::KeyboardModifiers mods, int keyQt) {
            bool retVal = false;
            QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                      "checkKeyPressed",
                                      Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, retVal),
                                      Q_ARG(int, int(mods) | keyQt));
            return retVal;
        };
        if (check(mods, keyQt)) {
            return true;
        } else if (keyQt == Qt::Key_Backtab) {
            // KGlobalAccel on X11 has some workaround for Backtab
            // see kglobalaccel/src/runtime/plugins/xcb/kglobalccel_x11.cpp method x11KeyPress
            // Apparently KKeySequenceWidget captures Shift+Tab instead of Backtab
            // thus if the key is backtab we should adjust to add shift again and use tab
            // in addition KWin registers the shortcut incorrectly as Alt+Shift+Backtab
            // this should be changed to either Alt+Backtab or Alt+Shift+Tab to match KKeySequenceWidget
            // trying the variants
            if (check(mods | Qt::ShiftModifier, keyQt)) {
                return true;
            }
            if (check(mods | Qt::ShiftModifier, Qt::Key_Tab)) {
                return true;
            }
        }
    }
#endif
    return false;
}

bool GlobalShortcutsManager::processKeyRelease(Qt::KeyboardModifiers mods, int keyQt)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    if (m_kglobalAccelInterface) {
        QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                  "checkKeyReleased",
                                  Qt::DirectConnection,
                                  Q_ARG(int, int(mods) | keyQt));
    }
#endif
    return false;
}

template<typename ShortcutKind, typename... Args>
bool match(QList<GlobalShortcut> &shortcuts, Args... args)
{
    for (auto &sc : shortcuts) {
        if (std::holds_alternative<ShortcutKind>(sc.shortcut())) {
            if (std::get<ShortcutKind>(sc.shortcut()) == ShortcutKind{args...}) {
                sc.invoke();
                return true;
            }
        }
    }
    return false;
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
    return match<PointerButtonShortcut>(m_shortcuts, mods, pointerButtons);
}

bool GlobalShortcutsManager::processAxis(Qt::KeyboardModifiers mods, PointerAxisDirection axis)
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
    return match<PointerAxisShortcut>(m_shortcuts, mods, axis);
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

} // namespace

#include "moc_globalshortcuts.cpp"
