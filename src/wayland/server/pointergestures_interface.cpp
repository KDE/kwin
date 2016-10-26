/****************************************************************************
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
****************************************************************************/
#include "pointergestures_interface_p.h"

namespace KWayland
{
namespace Server
{

PointerGesturesInterface::Private::Private(PointerGesturesInterfaceVersion interfaceVersion, PointerGesturesInterface *q, Display *d, const wl_interface *interface, quint32 version)
    : Global::Private(d, interface, version)
    , interfaceVersion(interfaceVersion)
    , q(q)
{
}

PointerGesturesInterface::PointerGesturesInterface(Private *d, QObject *parent)
    : Global(d, parent)
{
}

PointerGesturesInterface::~PointerGesturesInterface() = default;

PointerGesturesInterfaceVersion PointerGesturesInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

PointerGesturesInterface::Private *PointerGesturesInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

PointerSwipeGestureInterface::Private::Private(PointerSwipeGestureInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation, PointerInterface *pointer)
    : Resource::Private(q, c, parentResource, interface, implementation)
    , pointer(pointer)
{
}

PointerSwipeGestureInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

PointerSwipeGestureInterface::PointerSwipeGestureInterface(Private *p, QObject *parent)
    : Resource(p, parent)
{
}

PointerSwipeGestureInterface::~PointerSwipeGestureInterface() = default;

PointerSwipeGestureInterface::Private *PointerSwipeGestureInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

PointerPinchGestureInterface::Private::Private(PointerPinchGestureInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation, PointerInterface *pointer)
    : Resource::Private(q, c, parentResource, interface, implementation)
    , pointer(pointer)
{
}

PointerPinchGestureInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

PointerPinchGestureInterface::PointerPinchGestureInterface(Private *p, QObject *parent)
    : Resource(p, parent)
{
}

PointerPinchGestureInterface::~PointerPinchGestureInterface() = default;

PointerPinchGestureInterface::Private *PointerPinchGestureInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
