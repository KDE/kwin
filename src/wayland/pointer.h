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
#ifndef KWIN_WAYLAND_POINTER_H
#define KWIN_WAYLAND_POINTER_H

#include <QObject>
#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

class Surface;

class Pointer : public QObject
{
    Q_OBJECT
public:
    enum class ButtonState {
        Released,
        Pressed
    };
    enum class Axis {
        Vertical,
        Horizontal
    };
    explicit Pointer(QObject *parent = nullptr);
    virtual ~Pointer();

    bool isValid() const {
        return m_pointer;
    }
    void setup(wl_pointer *pointer);
    void release();

    Surface *enteredSurface() const {
        return m_enteredSurface;
    }
    Surface *enteredSurface(){
        return m_enteredSurface;
    }

    operator wl_pointer*() {
        return m_pointer;
    }
    operator wl_pointer*() const {
        return m_pointer;
    }

    static void enterCallback(void *data, wl_pointer *pointer, uint32_t serial, wl_surface *surface,
                              wl_fixed_t sx, wl_fixed_t sy);
    static void leaveCallback(void *data, wl_pointer *pointer, uint32_t serial, wl_surface *surface);
    static void motionCallback(void *data, wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
    static void buttonCallback(void *data, wl_pointer *pointer, uint32_t serial, uint32_t time,
                               uint32_t button, uint32_t state);
    static void axisCallback(void *data, wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value);

Q_SIGNALS:
    void entered(quint32 serial, const QPointF &relativeToSurface);
    void left(quint32 serial);
    void motion(const QPointF &relativeToSurface, quint32 time);
    void buttonStateChanged(quint32 serial, quint32 time, quint32 button, KWin::Wayland::Pointer::ButtonState state);
    void axisChanged(quint32 time, KWin::Wayland::Pointer::Axis axis, qreal delta);

private:
    void enter(uint32_t serial, wl_surface *surface, const QPointF &relativeToSurface);
    void leave(uint32_t serial);
    wl_pointer *m_pointer;
    Surface *m_enteredSurface;
    static const wl_pointer_listener s_listener;
};

}
}

Q_DECLARE_METATYPE(KWin::Wayland::Pointer::ButtonState)
Q_DECLARE_METATYPE(KWin::Wayland::Pointer::Axis)

#endif
