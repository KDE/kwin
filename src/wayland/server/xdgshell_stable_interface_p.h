/****************************************************************************
Copyright 2017 David Edmundson <davidedmundson@kde.org>

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
#ifndef KWAYLAND_SERVER_XDGSHELL_STABLE_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGSHELL_STABLE_INTERFACE_P_H

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
class XdgTopLevelStableInterface;
class XdgPopupStableInterface;
class XdgPositionerStableInterface;
class XdgSurfaceStableInterface;
template <typename T>
class GenericShellSurface;

class XdgShellStableInterface : public XdgShellInterface
{
    Q_OBJECT
public:
    virtual ~XdgShellStableInterface();

    /**
     * @returns The XdgTopLevelV6Interface for the @p native resource.
     **/
    XdgTopLevelStableInterface *getSurface(wl_resource *native);
    //DAVE we want to rename this, as it's bloody confusing. But XdgShellInterface::getSurface exists and expects that
    //also use a less terrible argument name than native. It's obvious it's native from the type

    XdgPositionerStableInterface *getPositioner(wl_resource *native);

    XdgSurfaceStableInterface *realGetSurface(wl_resource *native);

    Display *display() const;

    void ping(XdgShellSurfaceInterface * surface);

private:
    explicit XdgShellStableInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

class XdgSurfaceStableInterface : public KWayland::Server::Resource
{
    Q_OBJECT
public:
    virtual ~XdgSurfaceStableInterface();
    SurfaceInterface* surface() const;
    XdgTopLevelStableInterface* topLevel() const;
    XdgPopupStableInterface *popup() const;

private:
    explicit XdgSurfaceStableInterface(XdgShellStableInterface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellStableInterface;

    class Private;
    Private *d_func() const;
};

class XdgTopLevelStableInterface : public
XdgShellSurfaceInterface
{
    Q_OBJECT
public:
    virtual ~XdgTopLevelStableInterface();

private:
    explicit XdgTopLevelStableInterface(XdgShellStableInterface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellStableInterface;
    friend class XdgSurfaceStableInterface;

    class Private;
    Private *d_func() const;
};

class XdgPopupStableInterface : public XdgShellPopupInterface
{
    Q_OBJECT
public:
    virtual ~XdgPopupStableInterface();

private:
    explicit XdgPopupStableInterface(XdgShellStableInterface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellStableInterface;
    friend class XdgSurfaceStableInterface;
    friend class GenericShellSurface<XdgPopupStableInterface>;

    class Private;
    Private *d_func() const;
};

/*
 * This is a private internal class that keeps track of sent data
 * At the time of PopupCreation these values are copied to the popup
 */
class XdgPositionerStableInterface: public KWayland::Server::Resource
{
public:
    QSize initialSize() const;
    QRect anchorRect() const;
    Qt::Edges anchorEdge() const;
    Qt::Edges gravity() const;
    PositionerConstraints constraintAdjustments() const;
    QPoint anchorOffset() const;
private:
    explicit XdgPositionerStableInterface(XdgShellStableInterface *parent, wl_resource *parentResource);
    friend class XdgShellStableInterface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
