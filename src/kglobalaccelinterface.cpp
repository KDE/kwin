/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kglobalaccelinterface.h"

#include "input.h"

#include <QDebug>

KGlobalAccelImpl::KGlobalAccelImpl()
    : KGlobalAccelInterface()
{
}

KGlobalAccelImpl::~KGlobalAccelImpl() = default;

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

void KGlobalAccelImpl::cancelModiferOnlySequence()
{
    resetModifierOnlyState();
}

#include "moc_kglobalaccelinterface.cpp"
