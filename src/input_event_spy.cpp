/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input_event_spy.h"
#include "input.h"

#include <QPointF>

namespace KWin
{

InputEventSpy::InputEventSpy() = default;

InputEventSpy::~InputEventSpy()
{
    if (input()) {
        input()->uninstallInputEventSpy(this);
    }
}

void InputEventSpy::pointerMotion(PointerMotionEvent *event)
{
}

void InputEventSpy::pointerButton(PointerButtonEvent *event)
{
}

void InputEventSpy::pointerAxis(PointerAxisEvent *event)
{
}

void InputEventSpy::keyboardKey(KeyboardKeyEvent *event)
{
}

void InputEventSpy::touchDown(qint32 id, const QPointF &point, std::chrono::microseconds time)
{
}

void InputEventSpy::touchMotion(qint32 id, const QPointF &point, std::chrono::microseconds time)
{
}

void InputEventSpy::touchUp(qint32 id, std::chrono::microseconds time)
{
}

void InputEventSpy::pinchGestureBegin(int fingerCount, std::chrono::microseconds time)
{
}

void InputEventSpy::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time)
{
}

void InputEventSpy::pinchGestureEnd(std::chrono::microseconds time)
{
}

void InputEventSpy::pinchGestureCancelled(std::chrono::microseconds time)
{
}

void InputEventSpy::swipeGestureBegin(int fingerCount, std::chrono::microseconds time)
{
}

void InputEventSpy::swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time)
{
}

void InputEventSpy::swipeGestureEnd(std::chrono::microseconds time)
{
}

void InputEventSpy::swipeGestureCancelled(std::chrono::microseconds time)
{
}

void InputEventSpy::holdGestureBegin(int fingerCount, std::chrono::microseconds time)
{
}

void InputEventSpy::holdGestureEnd(std::chrono::microseconds time)
{
}

void InputEventSpy::holdGestureCancelled(std::chrono::microseconds time)
{
}

void InputEventSpy::switchEvent(SwitchEvent *event)
{
}

void InputEventSpy::tabletToolProximityEvent(TabletToolProximityEvent *event)
{
}

void InputEventSpy::tabletToolAxisEvent(TabletToolAxisEvent *event)
{
}

void InputEventSpy::tabletToolTipEvent(TabletToolTipEvent *event)
{
}

void InputEventSpy::tabletToolButtonEvent(TabletToolButtonEvent *event)
{
}

void InputEventSpy::tabletPadButtonEvent(TabletPadButtonEvent *event)
{
}

void InputEventSpy::tabletPadStripEvent(TabletPadStripEvent *event)
{
}

void InputEventSpy::tabletPadRingEvent(TabletPadRingEvent *event)
{
}
}
