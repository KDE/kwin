/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "globalshortcuts.h"
// kwin
#include <config-kwin.h>
#include "main.h"
#include "gestures.h"
#include "utils.h"
// KDE
#include <KGlobalAccel/private/kglobalacceld.h>
#include <KGlobalAccel/private/kglobalaccel_interface.h>
// Qt
#include <QAction>

namespace KWin
{

uint qHash(SwipeDirection direction)
{
    return uint(direction);
}

GlobalShortcut::GlobalShortcut(const QKeySequence &shortcut)
    : m_shortcut(shortcut)
    , m_pointerModifiers(Qt::NoModifier)
    , m_pointerButtons(Qt::NoButton)
{
}

GlobalShortcut::GlobalShortcut(Qt::KeyboardModifiers pointerButtonModifiers, Qt::MouseButtons pointerButtons)
    : m_shortcut(QKeySequence())
    , m_pointerModifiers(pointerButtonModifiers)
    , m_pointerButtons(pointerButtons)
{
}

GlobalShortcut::GlobalShortcut(Qt::KeyboardModifiers modifiers)
    : m_shortcut(QKeySequence())
    , m_pointerModifiers(modifiers)
    , m_pointerButtons(Qt::NoButton)
{
}

GlobalShortcut::GlobalShortcut(SwipeDirection direction)
    : m_shortcut(QKeySequence())
    , m_pointerModifiers(Qt::NoModifier)
    , m_pointerButtons(Qt::NoButton)
    , m_swipeDirection(direction)
{
}

GlobalShortcut::~GlobalShortcut()
{
}

InternalGlobalShortcut::InternalGlobalShortcut(Qt::KeyboardModifiers modifiers, const QKeySequence &shortcut, QAction *action)
    : GlobalShortcut(shortcut)
    , m_action(action)
{
    Q_UNUSED(modifiers)
}

InternalGlobalShortcut::InternalGlobalShortcut(Qt::KeyboardModifiers pointerButtonModifiers, Qt::MouseButtons pointerButtons, QAction *action)
    : GlobalShortcut(pointerButtonModifiers, pointerButtons)
    , m_action(action)
{
}

InternalGlobalShortcut::InternalGlobalShortcut(Qt::KeyboardModifiers axisModifiers, PointerAxisDirection axis, QAction *action)
    : GlobalShortcut(axisModifiers)
    , m_action(action)
{
    Q_UNUSED(axis)
}

static SwipeGesture::Direction toSwipeDirection(SwipeDirection direction)
{
    switch (direction) {
    case SwipeDirection::Up:
        return SwipeGesture::Direction::Up;
    case SwipeDirection::Down:
        return SwipeGesture::Direction::Down;
    case SwipeDirection::Left:
        return SwipeGesture::Direction::Left;
    case SwipeDirection::Right:
        return SwipeGesture::Direction::Right;
    case SwipeDirection::Invalid:
    default:
        Q_UNREACHABLE();
    }
}

InternalGlobalShortcut::InternalGlobalShortcut(Qt::KeyboardModifiers swipeModifier, SwipeDirection direction, QAction *action)
    : GlobalShortcut(direction)
    , m_action(action)
    , m_swipe(new SwipeGesture)
{
    Q_UNUSED(swipeModifier)
    m_swipe->setDirection(toSwipeDirection(direction));
    m_swipe->setMinimumFingerCount(4);
    m_swipe->setMaximumFingerCount(4);
    QObject::connect(m_swipe.data(), &SwipeGesture::triggered, m_action, &QAction::trigger, Qt::QueuedConnection);
}

InternalGlobalShortcut::~InternalGlobalShortcut()
{
}

void InternalGlobalShortcut::invoke()
{
    // using QueuedConnection so that we finish the even processing first
    QMetaObject::invokeMethod(m_action, "trigger", Qt::QueuedConnection);
}

GlobalShortcutsManager::GlobalShortcutsManager(QObject *parent)
    : QObject(parent)
    , m_gestureRecognizer(new GestureRecognizer(this))
{
}

template <typename T>
void clearShortcuts(T &shortcuts)
{
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        qDeleteAll((*it));
    }
}

GlobalShortcutsManager::~GlobalShortcutsManager()
{
    clearShortcuts(m_pointerShortcuts);
    clearShortcuts(m_axisShortcuts);
    clearShortcuts(m_swipeShortcuts);
}

void GlobalShortcutsManager::init()
{
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        qputenv("KGLOBALACCELD_PLATFORM", QByteArrayLiteral("org.kde.kwin"));
        m_kglobalAccel = new KGlobalAccelD(this);
        if (!m_kglobalAccel->init()) {
            qCDebug(KWIN_CORE) << "Init of kglobalaccel failed";
            delete m_kglobalAccel;
            m_kglobalAccel = nullptr;
        } else {
            qCDebug(KWIN_CORE) << "KGlobalAcceld inited";
        }
    }
}

template <typename T>
void handleDestroyedAction(QObject *object, T &shortcuts)
{
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        auto &list = it.value();
        auto it2 = list.begin();
        while (it2 != list.end()) {
            if (InternalGlobalShortcut *shortcut = dynamic_cast<InternalGlobalShortcut*>(it2.value())) {
                if (shortcut->action() == object) {
                    it2 = list.erase(it2);
                    delete shortcut;
                    continue;
                }
            }
            ++it2;
        }
    }
}

void GlobalShortcutsManager::objectDeleted(QObject *object)
{
    handleDestroyedAction(object, m_pointerShortcuts);
    handleDestroyedAction(object, m_axisShortcuts);
    handleDestroyedAction(object, m_swipeShortcuts);
}

template <typename T, typename R>
GlobalShortcut *addShortcut(T &shortcuts, QAction *action, Qt::KeyboardModifiers modifiers, R value)
{
    GlobalShortcut *cut = new InternalGlobalShortcut(modifiers, value, action);
    auto it = shortcuts.find(modifiers);
    if (it != shortcuts.end()) {
        // TODO: check if shortcut already exists
        (*it).insert(value, cut);
    } else {
        QHash<R, GlobalShortcut*> s;
        s.insert(value, cut);
        shortcuts.insert(modifiers, s);
    }
    return cut;
}

void GlobalShortcutsManager::registerPointerShortcut(QAction *action, Qt::KeyboardModifiers modifiers, Qt::MouseButtons pointerButtons)
{
    addShortcut(m_pointerShortcuts, action, modifiers, pointerButtons);
    connect(action, &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
}

void GlobalShortcutsManager::registerAxisShortcut(QAction *action, Qt::KeyboardModifiers modifiers, PointerAxisDirection axis)
{
    addShortcut(m_axisShortcuts, action, modifiers, axis);
    connect(action, &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
}

void GlobalShortcutsManager::registerTouchpadSwipe(QAction *action, SwipeDirection direction)
{
    auto shortcut = addShortcut(m_swipeShortcuts, action, Qt::NoModifier, direction);
    connect(action, &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
    m_gestureRecognizer->registerGesture(static_cast<InternalGlobalShortcut*>(shortcut)->swipeGesture());
}

template <typename T, typename U>
bool processShortcut(Qt::KeyboardModifiers mods, T key, U &shortcuts)
{
    auto it = shortcuts.find(mods);
    if (it == shortcuts.end()) {
        return false;
    }
    auto it2 = (*it).find(key);
    if (it2 == (*it).end()) {
        return false;
    }
    it2.value()->invoke();
    return true;
}

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, int keyQt)
{
    if (m_kglobalAccelInterface) {
        if (!keyQt && !mods) {
            return false;
        }
        auto check = [this] (Qt::KeyboardModifiers mods, int keyQt) {
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

bool GlobalShortcutsManager::processPointerPressed(Qt::KeyboardModifiers mods, Qt::MouseButtons pointerButtons)
{
    return processShortcut(mods, pointerButtons, m_pointerShortcuts);
}

bool GlobalShortcutsManager::processAxis(Qt::KeyboardModifiers mods, PointerAxisDirection axis)
{
    return processShortcut(mods, axis, m_axisShortcuts);
}

void GlobalShortcutsManager::processSwipeStart(uint fingerCount)
{
    m_gestureRecognizer->startSwipeGesture(fingerCount);
}

void GlobalShortcutsManager::processSwipeUpdate(const QSizeF &delta)
{
    m_gestureRecognizer->updateSwipeGesture(delta);
}

void GlobalShortcutsManager::processSwipeCancel()
{
    m_gestureRecognizer->cancelSwipeGesture();
}

void GlobalShortcutsManager::processSwipeEnd()
{
    m_gestureRecognizer->endSwipeGesture();
    // TODO: cancel on Wayland Seat if one triggered
}

} // namespace
