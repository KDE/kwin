/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDGSHELL_V6_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGSHELL_V6_INTERFACE_P_H

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
class XdgTopLevelV6Interface;
class XdgPopupV6Interface;
class XdgPositionerV6Interface;
class XdgSurfaceV6Interface;
template <typename T>
class GenericShellSurface;

class XdgShellV6Interface : public XdgShellInterface
{
    Q_OBJECT
public:
    virtual ~XdgShellV6Interface();

    /**
     * @returns The XdgTopLevelV6Interface for the @p native resource.
     **/
    XdgTopLevelV6Interface *getSurface(wl_resource *native);
    //DAVE we want to rename this, as it's bloody confusing. But XdgShellInterface::getSurface exists and expects that
    //also use a less terrible argument name than native. It's obvious it's native from the type

    XdgPositionerV6Interface *getPositioner(wl_resource *native);

    XdgSurfaceV6Interface *realGetSurface(wl_resource *native);

    Display *display() const;

    void ping(XdgShellSurfaceInterface * surface);

private:
    explicit XdgShellV6Interface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

class XdgSurfaceV6Interface : public KWayland::Server::Resource
{
    Q_OBJECT
public:
    virtual ~XdgSurfaceV6Interface();
    SurfaceInterface* surface() const;
    XdgTopLevelV6Interface* topLevel() const;
    XdgPopupV6Interface *popup() const;

private:
    explicit XdgSurfaceV6Interface(XdgShellV6Interface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellV6Interface;

    class Private;
    Private *d_func() const;
};

class XdgTopLevelV6Interface : public
XdgShellSurfaceInterface
{
    Q_OBJECT
public:
    virtual ~XdgTopLevelV6Interface();

private:
    explicit XdgTopLevelV6Interface(XdgShellV6Interface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellV6Interface;
    friend class XdgSurfaceV6Interface;

    class Private;
    Private *d_func() const;
};

class XdgPopupV6Interface : public XdgShellPopupInterface
{
    Q_OBJECT
public:
    virtual ~XdgPopupV6Interface();

private:
    explicit XdgPopupV6Interface(XdgShellV6Interface *parent, SurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgShellV6Interface;
    friend class XdgSurfaceV6Interface;
    friend class GenericShellSurface<XdgPopupV6Interface>;

    class Private;
    Private *d_func() const;
};

/*
 * This is a private internal class that keeps track of sent data
 * At the time of PopupCreation these values are copied to the popup
 */
class XdgPositionerV6Interface: public KWayland::Server::Resource
{
public:
    QSize initialSize() const;
    QRect anchorRect() const;
    Qt::Edges anchorEdge() const;
    Qt::Edges gravity() const;
    PositionerConstraints constraintAdjustments() const;
    QPoint anchorOffset() const;
private:
    explicit XdgPositionerV6Interface(XdgShellV6Interface *parent, wl_resource *parentResource);
    friend class XdgShellV6Interface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
