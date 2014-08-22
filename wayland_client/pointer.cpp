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
#include "pointer.h"
#include "surface.h"

#include <QPointF>

namespace KWin
{
namespace Wayland
{

const wl_pointer_listener Pointer::s_listener = {
    Pointer::enterCallback,
    Pointer::leaveCallback,
    Pointer::motionCallback,
    Pointer::buttonCallback,
    Pointer::axisCallback
};

Pointer::Pointer(QObject *parent)
    : QObject(parent)
    , m_pointer(nullptr)
    , m_enteredSurface(nullptr)
{
}

Pointer::~Pointer()
{
    release();
}

void Pointer::release()
{
    if (!m_pointer) {
        return;
    }
    wl_pointer_destroy(m_pointer);
    m_pointer = nullptr;
}

void Pointer::setup(wl_pointer *pointer)
{
    Q_ASSERT(pointer);
    Q_ASSERT(!m_pointer);
    m_pointer = pointer;
    wl_pointer_add_listener(m_pointer, &Pointer::s_listener, this);
}

void Pointer::enterCallback(void *data, wl_pointer *pointer, uint32_t serial, wl_surface *surface,
                            wl_fixed_t sx, wl_fixed_t sy)
{
    Pointer *p = reinterpret_cast<Pointer*>(data);
    Q_ASSERT(p->m_pointer == pointer);
    p->enter(serial, surface, QPointF(wl_fixed_to_double(sx), wl_fixed_to_double(sy)));
}

void Pointer::enter(uint32_t serial, wl_surface *surface, const QPointF &relativeToSurface)
{
    m_enteredSurface = Surface::get(surface);
    emit entered(serial, relativeToSurface);
}

void Pointer::leaveCallback(void *data, wl_pointer *pointer, uint32_t serial, wl_surface *surface)
{
    Pointer *p = reinterpret_cast<Pointer*>(data);
    Q_ASSERT(p->m_pointer == pointer);
    Q_ASSERT(*(p->m_enteredSurface) == surface);
    p->leave(serial);
}

void Pointer::leave(uint32_t serial)
{
    m_enteredSurface = nullptr;
    emit left(serial);
}

void Pointer::motionCallback(void *data, wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    Pointer *p = reinterpret_cast<Pointer*>(data);
    Q_ASSERT(p->m_pointer == pointer);
    emit p->motion(QPointF(wl_fixed_to_double(sx), wl_fixed_to_double(sy)), time);
}

void Pointer::buttonCallback(void *data, wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    Pointer *p = reinterpret_cast<Pointer*>(data);
    Q_ASSERT(p->m_pointer == pointer);
    auto toState = [state] {
        if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
            return ButtonState::Released;
        } else {
            return ButtonState::Pressed;
        }
    };
    emit p->buttonStateChanged(serial, time, button, toState());
}

void Pointer::axisCallback(void *data, wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    Pointer *p = reinterpret_cast<Pointer*>(data);
    Q_ASSERT(p->m_pointer == pointer);
    auto toAxis = [axis] {
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            return Axis::Horizontal;
        } else {
            return Axis::Vertical;
        }
    };
    emit p->axisChanged(time, toAxis(), wl_fixed_to_double(value));
}

}
}
