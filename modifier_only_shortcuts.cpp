/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "modifier_only_shortcuts.h"
#include "input_event.h"
#include "options.h"
#include "screenlockerwatcher.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

namespace KWin
{

ModifierOnlyShortcuts::ModifierOnlyShortcuts()
    : QObject()
    , InputEventSpy()
{
    connect(ScreenLockerWatcher::self(), &ScreenLockerWatcher::locked, this, &ModifierOnlyShortcuts::reset);
}

ModifierOnlyShortcuts::~ModifierOnlyShortcuts() = default;

void ModifierOnlyShortcuts::keyEvent(KeyEvent *event)
{
    if (event->isAutoRepeat()) {
        return;
    }
    if (event->type() == QEvent::KeyPress) {
        const bool wasEmpty = m_pressedKeys.isEmpty();
        m_pressedKeys.insert(event->nativeScanCode());
        if (wasEmpty && m_pressedKeys.size() == 1 &&
            !ScreenLockerWatcher::self()->isLocked() &&
            m_buttonPressCount == 0 &&
            m_cachedMods == Qt::NoModifier) {
            m_modifier = Qt::KeyboardModifier(int(event->modifiersRelevantForGlobalShortcuts()));
        } else {
            m_modifier = Qt::NoModifier;
        }
    } else if (!m_pressedKeys.isEmpty()) {
        m_pressedKeys.remove(event->nativeScanCode());
        if (m_pressedKeys.isEmpty() &&
            event->modifiersRelevantForGlobalShortcuts() == Qt::NoModifier &&
            workspace() && !workspace()->globalShortcutsDisabled()) {
            if (m_modifier != Qt::NoModifier) {
                const auto list = options->modifierOnlyDBusShortcut(m_modifier);
                if (list.size() >= 4) {
                    if (!waylandServer() || !waylandServer()->isKeyboardShortcutsInhibited()) {
                        auto call = QDBusMessage::createMethodCall(list.at(0), list.at(1), list.at(2), list.at(3));
                        QVariantList args;
                        for (int i = 4; i < list.size(); ++i) {
                            args << list.at(i);
                        }
                        call.setArguments(args);
                        QDBusConnection::sessionBus().asyncCall(call);
                    }
                }
            }
        }
        m_modifier = Qt::NoModifier;
    } else {
        m_modifier = Qt::NoModifier;
    }
    m_cachedMods = event->modifiersRelevantForGlobalShortcuts();
}

void ModifierOnlyShortcuts::pointerEvent(MouseEvent *event)
{
    if (event->type() == QEvent::MouseMove) {
        return;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        m_buttonPressCount++;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_buttonPressCount--;
    }
    reset();
}

void ModifierOnlyShortcuts::wheelEvent(WheelEvent *event)
{
    Q_UNUSED(event)
    reset();
}

}
