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
#include "seat.h"
#include "keyboard.h"
#include "pointer.h"

namespace KWin
{
namespace Wayland
{

const wl_seat_listener Seat::s_listener = {
    Seat::capabilitiesCallback,
    Seat::nameCallback
};

Seat::Seat(QObject *parent)
    : QObject(parent)
    , m_seat(nullptr)
    , m_capabilityKeyboard(false)
    , m_capabilityPointer(false)
    , m_capabilityTouch(false)
{
}

Seat::~Seat()
{
    release();
}

void Seat::release()
{
    if (!m_seat) {
        return;
    }
    wl_seat_destroy(m_seat);
    m_seat = nullptr;
    resetSeat();
}

void Seat::destroy()
{
    if (!m_seat) {
        return;
    }
    free(m_seat);
    m_seat = nullptr;
    resetSeat();
}

void Seat::resetSeat()
{
    setHasKeyboard(false);
    setHasPointer(false);
    setHasTouch(false);
    setName(QString());
}

void Seat::setHasKeyboard(bool has)
{
    if (m_capabilityKeyboard == has) {
        return;
    }
    m_capabilityKeyboard = has;
    emit hasKeyboardChanged(m_capabilityKeyboard);
}

void Seat::setHasPointer(bool has)
{
    if (m_capabilityPointer == has) {
        return;
    }
    m_capabilityPointer = has;
    emit hasPointerChanged(m_capabilityPointer);
}

void Seat::setHasTouch(bool has)
{
    if (m_capabilityTouch == has) {
        return;
    }
    m_capabilityTouch = has;
    emit hasTouchChanged(m_capabilityTouch);
}

void Seat::setup(wl_seat *seat)
{
    Q_ASSERT(seat);
    Q_ASSERT(!m_seat);
    m_seat = seat;
    wl_seat_add_listener(m_seat, &Seat::s_listener, this);
}

void Seat::capabilitiesCallback(void *data, wl_seat *seat, uint32_t capabilities)
{
    Seat *s = reinterpret_cast<Seat*>(data);
    Q_ASSERT(s->m_seat == seat);
    s->capabilitiesChanged(capabilities);
}

void Seat::nameCallback(void *data, wl_seat *seat, const char *name)
{
    Seat *s = reinterpret_cast<Seat*>(data);
    Q_ASSERT(s->m_seat == seat);
    s->setName(QString::fromUtf8(name));
}

void Seat::capabilitiesChanged(uint32_t capabilities)
{
    setHasKeyboard(capabilities & WL_SEAT_CAPABILITY_KEYBOARD);
    setHasPointer(capabilities & WL_SEAT_CAPABILITY_POINTER);
    setHasTouch(capabilities & WL_SEAT_CAPABILITY_TOUCH);
}

Keyboard *Seat::createKeyboard(QObject *parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(m_capabilityKeyboard);
    Keyboard *k = new Keyboard(parent);
    k->setup(wl_seat_get_keyboard(m_seat));
    return k;
}

Pointer *Seat::createPointer(QObject *parent)
{
    Q_ASSERT(isValid());
    Q_ASSERT(m_capabilityPointer);
    Pointer *p = new Pointer(parent);
    p->setup(wl_seat_get_pointer(m_seat));
    return p;
}

wl_touch *Seat::createTouch()
{
    Q_ASSERT(isValid());
    Q_ASSERT(m_capabilityTouch);
    return wl_seat_get_touch(m_seat);
}

void Seat::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    emit nameChanged(m_name);
}

}
}
