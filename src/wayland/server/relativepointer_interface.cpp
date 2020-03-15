/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "relativepointer_interface_p.h"

namespace KWayland
{
namespace Server
{

RelativePointerManagerInterface::Private::Private(RelativePointerInterfaceVersion interfaceVersion, RelativePointerManagerInterface *q, Display *d, const wl_interface *interface, quint32 version)
    : Global::Private(d, interface, version)
    , interfaceVersion(interfaceVersion)
    , q(q)
{
}

RelativePointerManagerInterface::RelativePointerManagerInterface(Private *d, QObject *parent)
    : Global(d, parent)
{
}

RelativePointerManagerInterface::~RelativePointerManagerInterface() = default;

RelativePointerInterfaceVersion RelativePointerManagerInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

RelativePointerManagerInterface::Private *RelativePointerManagerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

RelativePointerInterface::Private::Private(RelativePointerInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation)
    : Resource::Private(q, c, parentResource, interface, implementation)
{
}

RelativePointerInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

RelativePointerInterface::RelativePointerInterface(Private *p, QObject *parent)
    : Resource(p, parent)
{
}

RelativePointerInterface::~RelativePointerInterface() = default;

void RelativePointerInterface::relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds)
{
    Q_D();
    d->relativeMotion(delta, deltaNonAccelerated, microseconds);
}

RelativePointerInterface::Private *RelativePointerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
