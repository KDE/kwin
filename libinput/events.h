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

class Event
{
public:
    virtual ~Event();

    libinput_event_type type() const;
    libinput_device *device() const;

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
    QPointF delta() const;
    uint32_t button() const;
    InputRedirection::PointerButtonState buttonState() const;
    uint32_t time() const;
    InputRedirection::PointerAxis axis() const;
    qreal axisValue() const;

    operator libinput_event_pointer*() {
        return m_pointerEvent;
    }
    operator libinput_event_pointer*() const {
        return m_pointerEvent;
    }

private:
    libinput_event_pointer *m_pointerEvent;
};

inline
libinput_event_type Event::type() const
{
    return m_type;
}

}
}

#endif
