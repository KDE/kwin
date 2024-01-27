/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputgrab.h"

namespace KWin
{

PointerInputGrab::~PointerInputGrab()
{
}

void PointerInputGrab::enter(Window *window, const QPointF &pos)
{
}

void PointerInputGrab::leave(Window *window)
{
}

void PointerInputGrab::frame()
{
}

void PointerInputGrab::motion(MouseEvent *event)
{
}

void PointerInputGrab::button(MouseEvent *event)
{
}

void PointerInputGrab::wheel(WheelEvent *event)
{
}

void PointerInputGrab::pinchGestureBegin(int fingerCount, std::chrono::microseconds time)
{
}

void PointerInputGrab::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time)
{
}

void PointerInputGrab::pinchGestureEnd(std::chrono::microseconds time)
{
}

void PointerInputGrab::pinchGestureCancelled(std::chrono::microseconds time)
{
}

void PointerInputGrab::swipeGestureBegin(int fingerCount, std::chrono::microseconds time)
{
}

void PointerInputGrab::swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time)
{
}

void PointerInputGrab::swipeGestureEnd(std::chrono::microseconds time)
{
}

void PointerInputGrab::swipeGestureCancelled(std::chrono::microseconds time)
{
}

void PointerInputGrab::holdGestureBegin(int fingerCount, std::chrono::microseconds time)
{
}

void PointerInputGrab::holdGestureEnd(std::chrono::microseconds time)
{
}

void PointerInputGrab::holdGestureCancelled(std::chrono::microseconds time)
{
}

KeyboardInputGrab::~KeyboardInputGrab()
{
}

void KeyboardInputGrab::key(KeyEvent *event)
{
}

TouchInputGrab::~TouchInputGrab()
{
}

void TouchInputGrab::enter(Window *window, const QPointF &pos)
{
}

void TouchInputGrab::leave(Window *window)
{
}

void TouchInputGrab::cancel()
{
}

void TouchInputGrab::frame()
{
}

void TouchInputGrab::motion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
}

void TouchInputGrab::down(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
}

void TouchInputGrab::up(qint32 id, std::chrono::microseconds time)
{
}

TabletInputGrab::~TabletInputGrab()
{
}

void TabletInputGrab::tool(TabletEvent *event)
{
}

void TabletInputGrab::toolButton(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time)
{
}

void TabletInputGrab::padButton(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
}

void TabletInputGrab::padStrip(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
}

void TabletInputGrab::padRing(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
}

} // namespace KWin
