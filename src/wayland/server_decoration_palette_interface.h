/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_DECORATION_PALETTE_INTERFACE_H
#define KWAYLAND_SERVER_DECORATION_PALETTE_INTERFACE_H

struct wl_resource;

#include <QObject>
#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class Display;
class SurfaceInterface;
class ServerSideDecorationPaletteInterface;
class ServerSideDecorationPaletteManagerInterfacePrivate;
class ServerSideDecorationPaletteInterfacePrivate;

/**
 * Allows a client to specify a preferred palette to use for server-side window decorations
 *
 * This global can be used for clients to bind ServerSideDecorationPaletteInterface instances
 * and notifies when a new one is created
 * @since 5.42
 */
class KWAYLANDSERVER_EXPORT ServerSideDecorationPaletteManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit ServerSideDecorationPaletteManagerInterface(Display *display, QObject *parent = nullptr);
    ~ServerSideDecorationPaletteManagerInterface() override;
    /**
     * Returns any existing palette for a given surface
     * This returns a null pointer if no ServerSideDecorationPaletteInterface exists.
     */
    ServerSideDecorationPaletteInterface* paletteForSurface(SurfaceInterface *);

Q_SIGNALS:
    /**
     * Emitted whenever a new ServerSideDecorationPaletteInterface is created.
     **/
    void paletteCreated(KWaylandServer::ServerSideDecorationPaletteInterface*);

private:
    QScopedPointer<ServerSideDecorationPaletteManagerInterfacePrivate> d;
};

/**
 * Provides the palette
 * This interface is attached to a wl_surface and informs the server of a requested palette
 * @since 5.42
 */
class KWAYLANDSERVER_EXPORT ServerSideDecorationPaletteInterface : public QObject
{
    Q_OBJECT
public:
    ~ServerSideDecorationPaletteInterface() override;

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
    explicit ServerSideDecorationPaletteInterface(SurfaceInterface *surface, wl_resource *resource);
    friend class ServerSideDecorationPaletteManagerInterfacePrivate;

    QScopedPointer<ServerSideDecorationPaletteInterfacePrivate> d;
};

}

#endif
