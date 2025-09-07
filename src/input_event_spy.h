/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <kwin_export.h>

#include <QtGlobal>
#include <chrono>

class QPointF;

namespace KWin
{
struct KeyboardKeyEvent;
struct PointerAxisEvent;
struct PointerButtonEvent;
struct PointerMotionEvent;
struct PointerSwipeGestureBeginEvent;
struct PointerSwipeGestureUpdateEvent;
struct PointerSwipeGestureEndEvent;
struct PointerSwipeGestureCancelEvent;
struct PointerPinchGestureBeginEvent;
struct PointerPinchGestureUpdateEvent;
struct PointerPinchGestureEndEvent;
struct PointerPinchGestureCancelEvent;
struct PointerHoldGestureBeginEvent;
struct PointerHoldGestureEndEvent;
struct PointerHoldGestureCancelEvent;
struct SwitchEvent;
struct TabletToolProximityEvent;
struct TabletToolAxisEvent;
struct TabletToolTipEvent;
struct TabletToolButtonEvent;
struct TabletPadButtonEvent;
struct TabletPadStripEvent;
struct TabletPadRingEvent;
struct TabletPadDialEvent;
struct TouchDownEvent;
struct TouchMotionEvent;
struct TouchUpEvent;

/**
 * Base class for spying on input events inside InputRedirection.
 *
 * This class is quite similar to InputEventFilter, except that it does not
 * support event filtering. Each InputEventSpy gets to see all input events,
 * the processing happens prior to sending events through the InputEventFilters.
 *
 * Deleting an instance of InputEventSpy automatically uninstalls it from
 * InputRedirection.
 */
class KWIN_EXPORT InputEventSpy
{
public:
    InputEventSpy();
    virtual ~InputEventSpy();

    virtual void pointerMotion(PointerMotionEvent *event);
    virtual void pointerButton(PointerButtonEvent *event);
    /**
     * Event spy for pointer axis events.
     *
     * @param event The event information about the axis event
     */
    virtual void pointerAxis(PointerAxisEvent *event);
    /**
     * Event spy for keyboard events.
     *
     * @param event The event information about the key event
     */
    virtual void keyboardKey(KeyboardKeyEvent *event);

    virtual void touchDown(TouchDownEvent *event);
    virtual void touchMotion(TouchMotionEvent *event);
    virtual void touchUp(TouchUpEvent *event);

    virtual void pinchGestureBegin(PointerPinchGestureBeginEvent *event);
    virtual void pinchGestureUpdate(PointerPinchGestureUpdateEvent *event);
    virtual void pinchGestureEnd(PointerPinchGestureEndEvent *event);
    virtual void pinchGestureCancelled(PointerPinchGestureCancelEvent *event);

    virtual void swipeGestureBegin(PointerSwipeGestureBeginEvent *event);
    virtual void swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event);
    virtual void swipeGestureEnd(PointerSwipeGestureEndEvent *event);
    virtual void swipeGestureCancelled(PointerSwipeGestureCancelEvent *event);

    virtual void holdGestureBegin(PointerHoldGestureBeginEvent *event);
    virtual void holdGestureEnd(PointerHoldGestureEndEvent *event);
    virtual void holdGestureCancelled(PointerHoldGestureCancelEvent *event);

    virtual void switchEvent(SwitchEvent *event);

    virtual void tabletToolProximityEvent(TabletToolProximityEvent *event);
    virtual void tabletToolAxisEvent(TabletToolAxisEvent *event);
    virtual void tabletToolTipEvent(TabletToolTipEvent *event);
    virtual void tabletToolButtonEvent(TabletToolButtonEvent *event);
    virtual void tabletPadButtonEvent(TabletPadButtonEvent *event);
    virtual void tabletPadStripEvent(TabletPadStripEvent *event);
    virtual void tabletPadRingEvent(TabletPadRingEvent *event);
    virtual void tabletPadDialEvent(TabletPadDialEvent *event);
};

} // namespace KWin
