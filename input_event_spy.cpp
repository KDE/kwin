/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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

void InputEventSpy::touchDown(quint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
}

void InputEventSpy::touchMotion(quint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
}

void InputEventSpy::touchUp(quint32 id, quint32 time)
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

}
