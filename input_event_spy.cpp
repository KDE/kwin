/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input_event_spy.h"
#include "input.h"

#include <QPointF>
#include <QSizeF>

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

void InputEventSpy::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
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

void InputEventSpy::swipeGestureUpdate(const QSizeF &delta, quint32 time)
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

void InputEventSpy::switchEvent(SwitchEvent *event)
{
    Q_UNUSED(event)
}

void InputEventSpy::tabletToolEvent(TabletEvent *event)
{
    Q_UNUSED(event)
}

void InputEventSpy::tabletToolButtonEvent(const QSet<uint> &pressedButtons)
{
    Q_UNUSED(pressedButtons)
}

void InputEventSpy::tabletPadButtonEvent(const QSet<uint> &pressedButtons)
{
    Q_UNUSED(pressedButtons)
}

void InputEventSpy::tabletPadStripEvent(int number, int position, bool isFinger)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
}

void InputEventSpy::tabletPadRingEvent(int number, int position, bool isFinger)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
}
}
