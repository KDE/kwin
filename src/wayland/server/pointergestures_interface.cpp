/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
