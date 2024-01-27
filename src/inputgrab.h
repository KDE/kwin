/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "input_event.h"

namespace KWin
{

/**
 * The PointerInputGrab type represents an exclusive pointer input handler.
 */
class KWIN_EXPORT PointerInputGrab
{
public:
    virtual ~PointerInputGrab();

    virtual void enter(Window *window, const QPointF &pos);
    virtual void leave(Window *window);

    virtual void frame();
    virtual void motion(MouseEvent *event);
    virtual void button(MouseEvent *event);
    virtual void wheel(WheelEvent *event);

    virtual void pinchGestureBegin(int fingerCount, std::chrono::microseconds time);
    virtual void pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time);
    virtual void pinchGestureEnd(std::chrono::microseconds time);
    virtual void pinchGestureCancelled(std::chrono::microseconds time);

    virtual void swipeGestureBegin(int fingerCount, std::chrono::microseconds time);
    virtual void swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time);
    virtual void swipeGestureEnd(std::chrono::microseconds time);
    virtual void swipeGestureCancelled(std::chrono::microseconds time);

    virtual void holdGestureBegin(int fingerCount, std::chrono::microseconds time);
    virtual void holdGestureEnd(std::chrono::microseconds time);
    virtual void holdGestureCancelled(std::chrono::microseconds time);
};

/**
 * THe KeyboardInputGrab type represents an exclusive keyboard input handler.
 */
class KWIN_EXPORT KeyboardInputGrab
{
public:
    virtual ~KeyboardInputGrab();

    virtual void key(KeyEvent *event);
};

/**
 * The TouchInputGrab type represents an exclusive touch input handler.
 */
class KWIN_EXPORT TouchInputGrab
{
public:
    virtual ~TouchInputGrab();

    virtual void enter(Window *window, const QPointF &pos);
    virtual void leave(Window *window);

    virtual void cancel();
    virtual void frame();
    virtual void motion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    virtual void down(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    virtual void up(qint32 id, std::chrono::microseconds time);
};

/**
 * The TabletInputGrab type represents an exclusive tablet input handler.
 */
class KWIN_EXPORT TabletInputGrab
{
public:
    virtual ~TabletInputGrab();

    virtual void tool(TabletEvent *event);
    virtual void toolButton(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time);
    virtual void padButton(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time);
    virtual void padStrip(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time);
    virtual void padRing(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time);
};

} // namespace KWin
