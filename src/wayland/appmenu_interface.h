/****************************************************************************
Copyright 2017  David Edmundson <kde@davidedmundson.co.uk>

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
#ifndef KWAYLAND_SERVER_APPMENU_INTERFACE_H
#define KWAYLAND_SERVER_APPMENU_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class AppMenuInterface;

/**
 * Provides the DBus service name and object path to a AppMenu DBus interface.
 *
 * This global can be used for clients to bind AppmenuInterface instances
 * and notifies when a new one is created
 * @since 5.42
 */
class KWAYLANDSERVER_EXPORT AppMenuManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~AppMenuManagerInterface();
    /**
     * Returns any existing appMenu for a given surface
     * This returns a null pointer if no AppMenuInterface exists.
     */
    AppMenuInterface* appMenuForSurface(SurfaceInterface *);

Q_SIGNALS:
    /**
     * Emitted whenever a new AppmenuInterface is created.
     **/
    void appMenuCreated(KWayland::Server::AppMenuInterface*);

private:
    explicit AppMenuManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

/**
 * Provides the DBus service name and object path to a AppMenu DBus interface.
 * This interface is attached to a wl_surface and provides access to where
 * the AppMenu DBus interface is registered.
 * @since 5.42
 */
class KWAYLANDSERVER_EXPORT AppMenuInterface : public Resource
{
    Q_OBJECT
public:
    /**
     * Structure containing DBus service name and path
     */
    struct InterfaceAddress {
        /** Service name of host with the AppMenu object*/
        QString serviceName;
        /** Object path of the AppMenu interface*/
        QString objectPath;
    };
    virtual ~AppMenuInterface();

    /**
     * @returns the service name and object path or empty strings if unset
     */
    InterfaceAddress address() const;

    /**
     * @returns The SurfaceInterface this AppmenuInterface references.
     **/
    SurfaceInterface *surface() const;

Q_SIGNALS:
    /**
     * Emitted when the address changes or is first received
     */
    void addressChanged(KWayland::Server::AppMenuInterface::InterfaceAddress);

private:
    explicit AppMenuInterface(AppMenuManagerInterface *parent, SurfaceInterface *s, wl_resource *parentResource);
    friend class AppMenuManagerInterface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
