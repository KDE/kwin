/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 KWin Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputeventscripting.h"

#include "core/inputdevice.h"

namespace KWin
{

InputEventScriptingSpy::InputEventScriptingSpy(QObject *parent)
    : QObject(parent)
    , InputEventSpy()
{
}

void InputEventScriptingSpy::pointerMotion(PointerMotionEvent *event)
{
    Q_EMIT mouseMoved(*event);
}

void InputEventScriptingSpy::pointerButton(PointerButtonEvent *event)
{
    if (event->state == PointerButtonState::Pressed) {
        Q_EMIT mousePressed(*event);
    } else if (event->state == PointerButtonState::Released) {
        Q_EMIT mouseReleased(*event);
    }
}

void InputEventScriptingSpy::keyboardKey(KeyboardKeyEvent *event)
{
    if (event->state == KeyboardKeyState::Pressed || event->state == KeyboardKeyState::Repeated) {
        Q_EMIT keyPressed(*event);
    } else if (event->state == KeyboardKeyState::Released) {
        Q_EMIT keyReleased(*event);
    }
}

} // namespace KWin
