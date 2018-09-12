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
#ifndef KWAYLAND_SERVER_DECORATION_PALETTE_INTERFACE_H
#define KWAYLAND_SERVER_DECORATION_PALETTE_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class ServerSideDecorationPaletteInterface;

/**
 * Allows a client to specify a preferred palette to use for server-side window decorations
 *
 * This global can be used for clients to bind ServerSideDecorationPaletteInterface instances
 * and notifies when a new one is created
 * @since 5.42
 */
class KWAYLANDSERVER_EXPORT ServerSideDecorationPaletteManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~ServerSideDecorationPaletteManagerInterface();
    /**
     * Returns any existing palette for a given surface
     * This returns a null pointer if no ServerSideDecorationPaletteInterface exists.
     */
    ServerSideDecorationPaletteInterface* paletteForSurface(SurfaceInterface *);

Q_SIGNALS:
    /**
     * Emitted whenever a new ServerSideDecorationPaletteInterface is created.
     **/
    void paletteCreated(KWayland::Server::ServerSideDecorationPaletteInterface*);

private:
    explicit ServerSideDecorationPaletteManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

/**
 * Provides the palette
 * This interface is attached to a wl_surface and informs the server of a requested palette
 * @since 5.42
 */
class KWAYLANDSERVER_EXPORT ServerSideDecorationPaletteInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~ServerSideDecorationPaletteInterface();

    /**
     * @returns the palette or an empty string if unset
     */
    QString palette() const;

    /**
     * @returns The SurfaceInterface this ServerSideDecorationPaletteInterface references.
     **/
    SurfaceInterface *surface() const;

Q_SIGNALS:
    /**
     * Emitted when the palette changes or is first received
     */
    void paletteChanged(const QString &palette);

private:
    explicit ServerSideDecorationPaletteInterface(ServerSideDecorationPaletteManagerInterface *parent, SurfaceInterface *s, wl_resource *parentResource);
    friend class ServerSideDecorationPaletteManagerInterface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
