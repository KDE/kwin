/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
