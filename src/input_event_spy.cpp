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

void InputEventSpy::pointerEvent(MouseEvent *event)
{
}

void InputEventSpy::wheelEvent(WheelEvent *event)
{
}

void InputEventSpy::keyEvent(KeyEvent *event)
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

void InputEventSpy::tabletToolEvent(TabletEvent *event)
{
}

void InputEventSpy::tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time)
{
}

void InputEventSpy::tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
}

void InputEventSpy::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
}

void InputEventSpy::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
}
}
