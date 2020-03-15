/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
