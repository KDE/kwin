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
struct SwitchEvent;
struct TabletToolProximityEvent;
struct TabletToolAxisEvent;
struct TabletToolTipEvent;
struct TabletToolButtonEvent;
struct TabletPadButtonEvent;
struct TabletPadStripEvent;
struct TabletPadRingEvent;

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
    virtual void touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    virtual void touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    virtual void touchUp(qint32 id, std::chrono::microseconds time);

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

    virtual void switchEvent(SwitchEvent *event);

    virtual void tabletToolProximityEvent(TabletToolProximityEvent *event);
    virtual void tabletToolAxisEvent(TabletToolAxisEvent *event);
    virtual void tabletToolTipEvent(TabletToolTipEvent *event);
    virtual void tabletToolButtonEvent(TabletToolButtonEvent *event);
    virtual void tabletPadButtonEvent(TabletPadButtonEvent *event);
    virtual void tabletPadStripEvent(TabletPadStripEvent *event);
    virtual void tabletPadRingEvent(TabletPadRingEvent *event);
};

} // namespace KWin
