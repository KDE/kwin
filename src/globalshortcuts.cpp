/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "globalshortcuts.h"
// config
#include <config-kwin.h>
// kwin
#include "gestures.h"
#include "kwinglobals.h"
#include "main.h"
#include "utils/common.h"
// KDE
#include <KGlobalAccel/private/kglobalaccel_interface.h>
#include <KGlobalAccel/private/kglobalacceld.h>
// Qt
#include <QAction>
// system
#include <signal.h>
#include <variant>

#include <iostream>

namespace KWin
{
GlobalShortcut::GlobalShortcut(Shortcut &&sc, QAction *action)
    : m_shortcut(std::move(sc))
    , m_action(action)
{
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
    QMetaObject::invokeMethod(m_action, "trigger", Qt::QueuedConnection);
}

const Shortcut &GlobalShortcut::shortcut() const
{
    return m_shortcut;
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

bool GlobalShortcutsManager::addIfNotExists(GlobalShortcut sc, GestureDeviceType device)
{
    for (const auto &cs : qAsConst(m_shortcuts)) {
        if (sc.shortcut() == cs.shortcut()) {
            return false;
        }
    }

    // Register if GestureSHortcut
    const auto &recognizer = device == GestureDeviceType::Touchpad ? m_touchpadGestureRecognizer : m_touchscreenGestureRecognizer;
    if (std::holds_alternative<GestureShortcut>(sc.shortcut())) {
        const GestureShortcut *shortcut = &std::get<GestureShortcut>(sc.shortcut());
        if (shortcut->swipeGesture) {
            recognizer->registerSwipeGesture(shortcut->swipeGesture.get());
        } else {
            recognizer->registerPinchGesture(shortcut->pinchGesture.get());
        }
    }
    connect(sc.action(), &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
    m_shortcuts.push_back(std::move(sc));
    return true;
}

void GlobalShortcutsManager::registerPointerShortcut(QAction *action, Qt::KeyboardModifiers modifiers, Qt::MouseButtons pointerButtons)
{
    addIfNotExists(GlobalShortcut(PointerButtonShortcut{modifiers, pointerButtons}, action));
}

void GlobalShortcutsManager::registerAxisShortcut(QAction *action, Qt::KeyboardModifiers modifiers, PointerAxisDirection axis)
{
    addIfNotExists(GlobalShortcut(PointerAxisShortcut{modifiers, axis}, action));
}

void GlobalShortcutsManager::registerGesture(GestureDeviceType device, GestureDirections direction, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)
{
    // Create and setup the GestureShortcut
    GestureShortcut shortcut{device, direction};
    if (isSwipeDirection(direction)) {
        std::unique_ptr<SwipeGesture> gesture = std::make_unique<SwipeGesture>();
        gesture->addFingerCount(fingerCount);
        gesture->setTriggerDelta(QSizeF(200, 200));
        connect(gesture.get(), &SwipeGesture::triggerProgress, progressCallback);
        connect(gesture.get(), &Gesture::triggered, onUp, &QAction::trigger, Qt::QueuedConnection);
        connect(gesture.get(), &Gesture::cancelled, onUp, &QAction::trigger, Qt::QueuedConnection);
        shortcut.swipeGesture = std::move(gesture);
    } else if (isPinchDirection(direction)) {
        std::unique_ptr<PinchGesture> gesture = std::make_unique<PinchGesture>();
        gesture->addFingerCount(fingerCount);
        connect(gesture.get(), &PinchGesture::triggerProgress, progressCallback);
        connect(gesture.get(), &Gesture::triggered, onUp, &QAction::trigger, Qt::QueuedConnection);
        connect(gesture.get(), &Gesture::cancelled, onUp, &QAction::trigger, Qt::QueuedConnection);
        shortcut.pinchGesture = std::move(gesture);
    }
    shortcut.gesture()->setDirection(direction);

    addIfNotExists(GlobalShortcut(std::move(shortcut), onUp), device);
}

void GlobalShortcutsManager::forceRegisterTouchscreenSwipe(QAction *onUp, std::function<void(qreal)> progressCallback, GestureDirection direction, uint fingerCount)
{
    std::unique_ptr<SwipeGesture> gesture = std::make_unique<SwipeGesture>();
    gesture->addFingerCount(fingerCount);
    gesture->setDirection(direction);
    gesture->setTriggerDelta(QSizeF(200, 200));
    connect(gesture.get(), &SwipeGesture::triggerProgress, progressCallback);
    connect(gesture.get(), &Gesture::triggered, onUp, &QAction::trigger, Qt::QueuedConnection);
    connect(gesture.get(), &Gesture::cancelled, onUp, &QAction::trigger, Qt::QueuedConnection);
    GestureShortcut gestureShortcut{GestureDeviceType::Touchscreen, direction};
    gestureShortcut.swipeGesture = std::move(gesture);
    m_touchscreenGestureRecognizer->registerSwipeGesture(gestureShortcut.swipeGesture.get());

    GlobalShortcut shortcut{std::move(gestureShortcut), onUp};
    const auto it = std::find_if(m_shortcuts.begin(), m_shortcuts.end(), [&shortcut](const auto &s) {
        return shortcut.shortcut() == s.shortcut();
    });
    if (it != m_shortcuts.end()) {
        m_shortcuts.erase(it);
    }
    connect(shortcut.action(), &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
    m_shortcuts.push_back(std::move(shortcut));
}

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, int keyQt)
{
    if (m_kglobalAccelInterface) {
        if (!keyQt && !mods) {
            return false;
        }
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
    return false;
}

bool GlobalShortcutsManager::processKeyRelease(Qt::KeyboardModifiers mods, int keyQt)
{
    if (m_kglobalAccelInterface) {
        QMetaObject::invokeMethod(m_kglobalAccelInterface,
                                  "checkKeyReleased",
                                  Qt::DirectConnection,
                                  Q_ARG(int, int(mods) | keyQt));
    }
    return false;
}

template<typename ShortcutKind, typename... Args>
bool match(std::vector<GlobalShortcut> &shortcuts, Args... args)
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
    return match<PointerButtonShortcut>(m_shortcuts, mods, pointerButtons);
}

bool GlobalShortcutsManager::processAxis(Qt::KeyboardModifiers mods, PointerAxisDirection axis)
{
    return match<PointerAxisShortcut>(m_shortcuts, mods, axis);
}

void GlobalShortcutsManager::processSwipeStart(GestureDeviceType device, uint fingerCount)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->startSwipeGesture(fingerCount);
    } else {
        m_touchscreenGestureRecognizer->startSwipeGesture(fingerCount);
    }
}

void GlobalShortcutsManager::processSwipeUpdate(GestureDeviceType device, const QSizeF &delta)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->updateSwipeGesture(delta);
    } else {
        m_touchscreenGestureRecognizer->updateSwipeGesture(delta);
    }
}

void GlobalShortcutsManager::processSwipeCancel(GestureDeviceType device)
{
    if (device == GestureDeviceType::Touchpad) {
        m_touchpadGestureRecognizer->cancelSwipeGesture();
    } else {
        m_touchscreenGestureRecognizer->cancelSwipeGesture();
    }
}

void GlobalShortcutsManager::processSwipeEnd(GestureDeviceType device)
{
    if (device == GestureDeviceType::Touchpad) {
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

void GlobalShortcutsManager::processPinchUpdate(qreal scale, qreal angleDelta, const QSizeF &delta)
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

Gesture *GestureShortcut::gesture() const
{
    if (swipeGesture != nullptr) {
        return swipeGesture.get();
    } else {
        return pinchGesture.get();
    }
}

} // namespace
