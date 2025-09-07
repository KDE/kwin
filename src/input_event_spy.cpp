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

void InputEventSpy::touchDown(TouchDownEvent *event)
{
}

void InputEventSpy::touchMotion(TouchMotionEvent *event)
{
}

void InputEventSpy::touchUp(TouchUpEvent *event)
{
}

void InputEventSpy::pinchGestureBegin(PointerPinchGestureBeginEvent *event)
{
}

void InputEventSpy::pinchGestureUpdate(PointerPinchGestureUpdateEvent *event)
{
}

void InputEventSpy::pinchGestureEnd(PointerPinchGestureEndEvent *event)
{
}

void InputEventSpy::pinchGestureCancelled(PointerPinchGestureCancelEvent *event)
{
}

void InputEventSpy::swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
{
}

void InputEventSpy::swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
{
}

void InputEventSpy::swipeGestureEnd(PointerSwipeGestureEndEvent *event)
{
}

void InputEventSpy::swipeGestureCancelled(PointerSwipeGestureCancelEvent *event)
{
}

void InputEventSpy::holdGestureBegin(PointerHoldGestureBeginEvent *event)
{
}

void InputEventSpy::holdGestureEnd(PointerHoldGestureEndEvent *event)
{
}

void InputEventSpy::holdGestureCancelled(PointerHoldGestureCancelEvent *event)
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

void InputEventSpy::tabletPadDialEvent(TabletPadDialEvent *event)
{
}
}
