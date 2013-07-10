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
// KDE
#include <kkeyserver.h>
#include <KConfigGroup>
// Qt
#include <QAction>

namespace KWin
{

GlobalShortcut::GlobalShortcut(const QKeySequence &shortcut)
    : m_shortcut(shortcut)
{
}

GlobalShortcut::~GlobalShortcut()
{
}

InternalGlobalShortcut::InternalGlobalShortcut(const QKeySequence &shortcut, QAction *action)
    : GlobalShortcut(shortcut)
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

GlobalShortcutsManager::~GlobalShortcutsManager()
{
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        qDeleteAll((*it));
    }
}

void GlobalShortcutsManager::objectDeleted(QObject *object)
{
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        auto list = (*it);
        for (auto it2 = list.begin(); it2 != list.end(); ++it2) {
            if (InternalGlobalShortcut *shortcut = dynamic_cast<InternalGlobalShortcut*>((*it2))) {
                if (shortcut->action() == object) {
                    delete *it2;
                    it2 = list.erase(it2);
                }
            }
        }
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
    GlobalShortcut *cut = new InternalGlobalShortcut(s, action);
    auto it = m_shortcuts.find(mods);
    if (it != m_shortcuts.end()) {
        // TODO: check if key already exists?
        (*it).insert(keysym, cut);
    } else {
        QHash<uint32_t, GlobalShortcut*> shortcuts;
        shortcuts.insert(keysym, cut);
        m_shortcuts.insert(mods, shortcuts);
    }
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

bool GlobalShortcutsManager::processKey(Qt::KeyboardModifiers mods, uint32_t key)
{
    auto it = m_shortcuts.find(mods);
    if (it == m_shortcuts.end()) {
        return false;
    }
    auto it2 = (*it).find(key);
    if (it2 == (*it).end()) {
        return false;
    }
    it2.value()->invoke();
    return true;
}

} // namespace
