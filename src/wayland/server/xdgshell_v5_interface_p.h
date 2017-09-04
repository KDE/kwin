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
#ifndef KWAYLAND_SERVER_XDGSHELL_V5_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGSHELL_V5_INTERFACE_P_H

#include "global.h"
#include "resource.h"
#include "xdgshell_interface.h"

#include <KWayland/Server/kwaylandserver_export.h>

#include <QSize>

namespace KWayland
{
namespace Server
{

class Display;
class OutputInterface;
class SeatInterface;
class SurfaceInterface;
class XdgPopupV5Interface;
class XdgSurfaceV5Interface;
template <typename T>
class GenericShellSurface;

class XdgShellV5Interface : public XdgShellInterface
{
    Q_OBJECT
public:
    virtual ~XdgShellV5Interface();

    /**
     * @returns The XdgSurfaceV5Interface for the @p native resource.
     **/
    XdgSurfaceV5Interface *getSurface(wl_resource *native);

    void ping();

private:
    explicit XdgShellV5Interface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

class XdgSurfaceV5Interface : public XdgShellSurfaceInterface
{
    Q_OBJECT
public:
    virtual ~XdgSurfaceV5Interface();

private:
    explicit XdgSurfaceV5Interface(XdgShellV5Interface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellV5Interface;

    class Private;
};

class XdgPopupV5Interface : public XdgShellPopupInterface
{
    Q_OBJECT
public:
    virtual ~XdgPopupV5Interface();

private:
    explicit XdgPopupV5Interface(XdgShellV5Interface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellV5Interface;
    friend class GenericShellSurface<XdgPopupV5Interface>;

    class Private;
    Private *d_func() const;
};

}
}

#endif
