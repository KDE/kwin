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
#ifndef KWAYLAND_SERVER_XDGSHELL_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGSHELL_INTERFACE_P_H
#include "xdgshell_interface.h"
#include "global_p.h"
#include "generic_shell_surface_p.h"
#include "resource_p.h"

namespace KWayland
{
namespace Server
{

class XdgShellInterface::Private : public Global::Private
{
public:
    XdgShellInterfaceVersion interfaceVersion;

protected:
    Private(XdgShellInterfaceVersion interfaceVersion, XdgShellInterface *q, Display *d, const wl_interface *interface, quint32 version);
    XdgShellInterface *q;
};

class XdgShellSurfaceInterface::Private : public Resource::Private, public GenericShellSurface<XdgShellSurfaceInterface>
{
public:
    virtual ~Private();

    virtual void close() = 0;
    virtual quint32 configure(States states, const QSize &size) = 0;

    XdgShellSurfaceInterface *q_func() {
        return reinterpret_cast<XdgShellSurfaceInterface *>(q);
    }

    QVector<quint32> configureSerials;
    QPointer<XdgShellSurfaceInterface> parent;
    XdgShellInterfaceVersion interfaceVersion;

protected:
    Private(XdgShellInterfaceVersion interfaceVersion, XdgShellSurfaceInterface *q, XdgShellInterface *c, SurfaceInterface *surface, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

};

class XdgShellPopupInterface::Private : public Resource::Private, public GenericShellSurface<XdgShellPopupInterface>
{
public:
    virtual ~Private();
    virtual void popupDone() = 0;

    XdgShellPopupInterface *q_func() {
        return reinterpret_cast<XdgShellPopupInterface *>(q);
    }

    QPointer<SurfaceInterface> parent;
    QPoint transientOffset;
    XdgShellInterfaceVersion interfaceVersion;

protected:
    Private(XdgShellInterfaceVersion interfaceVersion, XdgShellPopupInterface *q, XdgShellInterface *c, SurfaceInterface *surface, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

};

}
}

#endif
