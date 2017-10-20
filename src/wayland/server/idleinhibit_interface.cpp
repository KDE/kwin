/****************************************************************************
Copyright 2017  Martin Flöser <mgraesslin@kde.org>

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
#include "idleinhibit_interface_p.h"

namespace KWayland
{
namespace Server
{

IdleInhibitManagerInterface::Private::Private(IdleInhibitManagerInterface *q, Display *d, const wl_interface *interface, quint32 version, IdleInhibitManagerInterfaceVersion interfaceVersion)
    : Global::Private(d, interface, version)
    , interfaceVersion(interfaceVersion)
    , q(q)
{
}

IdleInhibitManagerInterface::IdleInhibitManagerInterface(Private *d, QObject *parent)
    : Global(d, parent)
{
}

IdleInhibitManagerInterface::~IdleInhibitManagerInterface() = default;

IdleInhibitManagerInterfaceVersion IdleInhibitManagerInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

IdleInhibitManagerInterface::Private *IdleInhibitManagerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
