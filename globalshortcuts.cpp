/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// own
#include "globalshortcuts.h"
// kwin
#include <config-kwin.h>
#include "main.h"
#include "utils.h"
// KDE
#include <kkeyserver.h>
#include <KConfigGroup>
#include <KGlobalAccel/private/kglobalacceld.h>
#include <KGlobalAccel/private/kglobalaccel_interface.h>
// Qt
#include <QAction>

namespace KWin
{

GlobalShortcut::GlobalShortcut(const QKeySequence &shortcut)
    : m_shortcut(shortcut)
    , m_pointerModifiers(Qt::NoModifier)
    , m_pointerButtons(Qt::NoButton)
    , m_axis(PointerAxisUp)
{
}

GlobalShortcut::GlobalShortcut(Qt::KeyboardModifiers pointerButtonModifiers, Qt::MouseButtons pointerButtons)
    : m_shortcut(QKeySequence())
    , m_pointerModifiers(pointerButtonModifiers)
    , m_pointerButtons(pointerButtons)
    , m_axis(PointerAxisUp)
{
}

GlobalShortcut::GlobalShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis)
    : m_shortcut(QKeySequence())
    , m_pointerModifiers(modifiers)
    , m_pointerButtons(Qt::NoButton)
    , m_axis(axis)
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
    : GlobalShortcut(axisModifiers, axis)
    , m_action(action)
{
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
    , m_config(KSharedConfig::openConfig(QStringLiteral("kglobalshortcutsrc"), KConfig::SimpleConfig))
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
    clearShortcuts(m_shortcuts);
    clearShortcuts(m_pointerShortcuts);
    clearShortcuts(m_axisShortcuts);
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
    handleDestroyedAction(object, m_shortcuts);
    handleDestroyedAction(object, m_pointerShortcuts);
    handleDestroyedAction(object, m_axisShortcuts);
}

template <typename T, typename R>
void addShortcut(T &shortcuts, QAction *action, Qt::KeyboardModifiers modifiers, R value)
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
}

void GlobalShortcutsManager::registerShortcut(QAction *action, const QKeySequence &shortcut)
{
    QKeySequence s = getShortcutForAction(KWIN_NAME, action->objectName(), shortcut);
    if (s.isEmpty()) {
        // TODO: insert into a list of empty shortcuts to react on changes
        return;
    }
    int keys = s[0];
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (keys & Qt::ShiftModifier) {
        mods |= Qt::ShiftModifier;
    }
    if (keys & Qt::ControlModifier) {
        mods |= Qt::ControlModifier;
    }
    if (keys & Qt::AltModifier) {
        mods |= Qt::AltModifier;
    }
    if (keys & Qt::MetaModifier) {
        mods |= Qt::MetaModifier;
    }
    int keysym = 0;
    if (!KKeyServer::keyQtToSymX(keys, &keysym)) {
        return;
    }
    addShortcut(m_shortcuts, action, mods, static_cast<uint32_t>(keysym));
    connect(action, &QAction::destroyed, this, &GlobalShortcutsManager::objectDeleted);
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

QKeySequence GlobalShortcutsManager::getShortcutForAction(const QString &componentName, const QString &actionName, const QKeySequence &defaultShortcut)
{
    if (!m_config->hasGroup(componentName)) {
        return defaultShortcut;
    }
    KConfigGroup group = m_config->group(componentName);
    if (!group.hasKey(actionName)) {
        return defaultShortcut;
    }
    QStringList parts = group.readEntry(actionName, QStringList());
    // must consist of three parts
    if (parts.size() != 3) {
        return defaultShortcut;
    }
    if (parts.first() == "none") {
        return defaultShortcut;
    }
    return QKeySequence(parts.first());
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

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, uint32_t key, int keyQt)
{
    if (m_kglobalAccelInterface) {
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
    if (processShortcut(mods, key, m_shortcuts)) {
        return true;
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

} // namespace
