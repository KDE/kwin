/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_LIBINPUT_EVENTS_H
#define KWIN_LIBINPUT_EVENTS_H

#include "../input.h"

#include <libinput.h>

namespace KWin
{
namespace LibInput
{

class Device;

class Event
{
public:
    virtual ~Event();

    libinput_event_type type() const;
    Device *device() const {
        return m_device;
    }
    libinput_device *nativeDevice() const;

    operator libinput_event*() {
        return m_event;
    }
    operator libinput_event*() const {
        return m_event;
    }

    static Event *create(libinput_event *event);

protected:
    Event(libinput_event *event, libinput_event_type type);

private:
    libinput_event *m_event;
    libinput_event_type m_type;
    Device *m_device;
};

class KeyEvent : public Event
{
public:
    KeyEvent(libinput_event *event);
    virtual ~KeyEvent();

    uint32_t key() const;
    InputRedirection::KeyboardKeyState state() const;
    uint32_t time() const;

    operator libinput_event_keyboard*() {
        return m_keyboardEvent;
    }
    operator libinput_event_keyboard*() const {
        return m_keyboardEvent;
    }

private:
    libinput_event_keyboard *m_keyboardEvent;
};

class PointerEvent : public Event
{
public:
    PointerEvent(libinput_event* event, libinput_event_type type);
    virtual ~PointerEvent();

    QPointF absolutePos() const;
    QPointF absolutePos(const QSize &size) const;
    QSizeF delta() const;
    QSizeF deltaUnaccelerated() const;
    uint32_t button() const;
    InputRedirection::PointerButtonState buttonState() const;
    uint32_t time() const;
    quint64 timeMicroseconds() const;
    QVector<InputRedirection::PointerAxis> axis() const;
    qreal axisValue(InputRedirection::PointerAxis a) const;

    operator libinput_event_pointer*() {
        return m_pointerEvent;
    }
    operator libinput_event_pointer*() const {
        return m_pointerEvent;
    }

private:
    libinput_event_pointer *m_pointerEvent;
};

class TouchEvent : public Event
{
public:
    TouchEvent(libinput_event *event, libinput_event_type type);
    virtual ~TouchEvent();

    quint32 time() const;
    QPointF absolutePos() const;
    QPointF absolutePos(const QSize &size) const;
    qint32 id() const;

    operator libinput_event_touch*() {
        return m_touchEvent;
    }
    operator libinput_event_touch*() const {
        return m_touchEvent;
    }

private:
    libinput_event_touch *m_touchEvent;
};

class GestureEvent : public Event
{
public:
    virtual ~GestureEvent();

    quint32 time() const;
    int fingerCount() const;

    QSizeF delta() const;

    bool isCancelled() const;

    operator libinput_event_gesture*() {
        return m_gestureEvent;
    }
    operator libinput_event_gesture*() const {
        return m_gestureEvent;
    }

protected:
    GestureEvent(libinput_event *event, libinput_event_type type);
    libinput_event_gesture *m_gestureEvent;
};

class PinchGestureEvent : public GestureEvent
{
public:
    PinchGestureEvent(libinput_event *event, libinput_event_type type);
    virtual ~PinchGestureEvent();

    qreal scale() const;
    qreal angleDelta() const;
};

class SwipeGestureEvent : public GestureEvent
{
public:
    SwipeGestureEvent(libinput_event *event, libinput_event_type type);
    virtual ~SwipeGestureEvent();
};

class SwitchEvent : public Event
{
public:
    SwitchEvent(libinput_event *event, libinput_event_type type);
    ~SwitchEvent() override;

    enum class State {
        Off,
        On
    };
    State state() const;

    quint32 time() const;
    quint64 timeMicroseconds() const;

private:
    libinput_event_switch *m_switchEvent;
};

inline
libinput_event_type Event::type() const
{
    return m_type;
}

}
}

#endif
