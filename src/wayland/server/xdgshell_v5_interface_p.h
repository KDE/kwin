/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
