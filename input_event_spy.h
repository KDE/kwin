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
#ifndef KWIN_INPUT_EVENT_SPY_H
#define KWIN_INPUT_EVENT_SPY_H
#include <kwin_export.h>

#include <QtGlobal>

class QPointF;
class QSizeF;

namespace KWin
{
class KeyEvent;
class MouseEvent;
class WheelEvent;
class SwitchEvent;


/**
 * Base class for spying on input events inside InputRedirection.
 *
 * This class is quite similar to InputEventFilter, except that it does not
 * support event filtering. Each InputEventSpy gets to see all input events,
 * the processing happens prior to sending events through the InputEventFilters.
 *
 * Deleting an instance of InputEventSpy automatically uninstalls it from
 * InputRedirection.
 **/
class KWIN_EXPORT InputEventSpy
{
public:
    InputEventSpy();
    virtual ~InputEventSpy();

    /**
     * Event spy for pointer events which can be described by a MouseEvent.
     *
     * @param event The event information about the move or button press/release
     **/
    virtual void pointerEvent(MouseEvent *event);
    /**
     * Event spy for pointer axis events.
     *
     * @param event The event information about the axis event
     **/
    virtual void wheelEvent(WheelEvent *event);
    /**
     * Event spy for keyboard events.
     *
     * @param event The event information about the key event
     **/
    virtual void keyEvent(KeyEvent *event);
    virtual void touchDown(quint32 id, const QPointF &pos, quint32 time);
    virtual void touchMotion(quint32 id, const QPointF &pos, quint32 time);
    virtual void touchUp(quint32 id, quint32 time);

    virtual void pinchGestureBegin(int fingerCount, quint32 time);
    virtual void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time);
    virtual void pinchGestureEnd(quint32 time);
    virtual void pinchGestureCancelled(quint32 time);

    virtual void swipeGestureBegin(int fingerCount, quint32 time);
    virtual void swipeGestureUpdate(const QSizeF &delta, quint32 time);
    virtual void swipeGestureEnd(quint32 time);
    virtual void swipeGestureCancelled(quint32 time);

    virtual void switchEvent(SwitchEvent *event);

};


} // namespace KWin

#endif
