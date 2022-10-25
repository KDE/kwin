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
    Q_UNUSED(event)
}

void InputEventSpy::wheelEvent(WheelEvent *event)
{
    Q_UNUSED(event)
}

void InputEventSpy::keyEvent(KeyEvent *event)
{
    Q_UNUSED(event)
}

void InputEventSpy::touchDown(qint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
}

void InputEventSpy::touchMotion(qint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
}

void InputEventSpy::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
}

void InputEventSpy::pinchGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
}

void InputEventSpy::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, quint32 time)
{
    Q_UNUSED(scale)
    Q_UNUSED(angleDelta)
    Q_UNUSED(delta)
    Q_UNUSED(time)
}

void InputEventSpy::pinchGestureEnd(quint32 time)
{
    Q_UNUSED(time)
}

void InputEventSpy::pinchGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
}

void InputEventSpy::swipeGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
}

void InputEventSpy::swipeGestureUpdate(const QPointF &delta, quint32 time)
{
    Q_UNUSED(delta)
    Q_UNUSED(time)
}

void InputEventSpy::swipeGestureEnd(quint32 time)
{
    Q_UNUSED(time)
}

void InputEventSpy::swipeGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
}

void InputEventSpy::holdGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
}

void InputEventSpy::holdGestureEnd(quint32 time)
{
    Q_UNUSED(time)
}

void InputEventSpy::holdGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
}

void InputEventSpy::switchEvent(SwitchEvent *event)
{
    Q_UNUSED(event)
}

void InputEventSpy::tabletToolEvent(TabletEvent *event)
{
    Q_UNUSED(event)
}

void InputEventSpy::tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, uint time)
{
    Q_UNUSED(button)
    Q_UNUSED(pressed)
    Q_UNUSED(tabletToolId)
    Q_UNUSED(time)
}

void InputEventSpy::tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, uint time)
{
    Q_UNUSED(button)
    Q_UNUSED(pressed)
    Q_UNUSED(tabletPadId)
    Q_UNUSED(time)
}

void InputEventSpy::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, uint time)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
    Q_UNUSED(tabletPadId)
    Q_UNUSED(time)
}

void InputEventSpy::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, uint time)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
    Q_UNUSED(tabletPadId)
    Q_UNUSED(time)
}
}
