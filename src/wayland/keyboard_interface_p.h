/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_KEYBOARD_INTERFACE_P_H
#define WAYLAND_SERVER_KEYBOARD_INTERFACE_P_H
#include "keyboard_interface.h"
#include "resource_p.h"

#include <QPointer>

namespace KWayland
{
namespace Server
{

class KeyboardInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *s, wl_resource *parentResource, KeyboardInterface *q);
    void sendKeymap();
    void sendKeymap(int fd, quint32 size);
    void sendModifiers();
    void sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial);

    void focusChildSurface(const QPointer<SurfaceInterface> &childSurface, quint32 serial);
    void sendLeave(SurfaceInterface *surface, quint32 serial);
    void sendEnter(SurfaceInterface *surface, quint32 serial);

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QPointer<SurfaceInterface> focusedChildSurface;
    QMetaObject::Connection destroyConnection;

private:
    static const struct wl_keyboard_interface s_interface;
};

}
}

#endif
