/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kglobalaccel_plugin.h"

#include "input.h"

#include <KGlobalShortcutTrigger>

#include <QDebug>

KGlobalAccelImpl::KGlobalAccelImpl(QObject *parent)
    : KGlobalAccelInterface(parent)
{
}

KGlobalAccelImpl::~KGlobalAccelImpl() = default;

bool KGlobalAccelImpl::grabKey(int key, bool grab)
{
    return true;
}

bool KGlobalAccelImpl::setTriggerActive(const KGlobalShortcutTrigger &trigger,
                                        bool active,
                                        const QString &componentName,
                                        const QString &actionId,
                                        const QString &componentFriendlyName,
                                        const QString &actionFriendlyName)
{
    Q_EMIT triggerActive(trigger.type(), trigger.serializedTriggerParams(), active, componentName, actionId, componentFriendlyName, actionFriendlyName);
    return true;
}

bool KGlobalAccelImpl::checkKeyPressed(int keyQt, KWin::KeyboardKeyState state)
{
    switch (state) {
    case KWin::KeyboardKeyState::Pressed:
        return keyEvent(keyQt, ShortcutKeyState::Pressed);
    case KWin::KeyboardKeyState::Repeated:
        return keyEvent(keyQt, ShortcutKeyState::Repeated);
    case KWin::KeyboardKeyState::Released:
        return keyEvent(keyQt, ShortcutKeyState::Released);
    }

    return false;
}

bool KGlobalAccelImpl::checkPointerPressed(Qt::MouseButtons buttons)
{
    return pointerPressed(buttons);
}

bool KGlobalAccelImpl::checkAxisTriggered(int axis)
{
    return axisTriggered(axis);
}

bool KGlobalAccelImpl::checkTriggerEvent(const QString &triggerType, const QString &serializedTriggerParams, int shortcutTriggerEventEnum)
{
    return triggerEvent(KGlobalShortcutTrigger(triggerType, serializedTriggerParams),
                        static_cast<ShortcutTriggerEvent>(shortcutTriggerEventEnum));
}

void KGlobalAccelImpl::cancelModiferOnlySequence()
{
    resetModifierOnlyState();
}

#include "moc_kglobalaccel_plugin.cpp"
