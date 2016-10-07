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
