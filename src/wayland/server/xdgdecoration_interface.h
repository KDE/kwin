/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDG_DECORATION_UNSTABLE_V1_H
#define KWAYLAND_SERVER_XDG_DECORATION_UNSTABLE_V1_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;
class XdgDecorationInterface;
class XdgShellInterface;
class XdgShellSurfaceInterface;

/**
 * @brief The XdgDecorationManagerInterface class
 * @since 5.54
 */
class KWAYLANDSERVER_EXPORT XdgDecorationManagerInterface : public Global
{
    Q_OBJECT
public:
    ~XdgDecorationManagerInterface() override;
Q_SIGNALS:
    void xdgDecorationInterfaceCreated(XdgDecorationInterface *iface);
private:
    explicit XdgDecorationManagerInterface(Display *display, XdgShellInterface *shellInterface, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

/**
 * @brief The XdgDecorationInterface class
 * @since 5.54
 */
class KWAYLANDSERVER_EXPORT XdgDecorationInterface : public Resource
{
    Q_OBJECT
public:
    enum class Mode {
        Undefined,
        ClientSide,
        ServerSide
    };

    Q_ENUM(Mode);

    ~XdgDecorationInterface() override;

    /**
     * Sets the mode the client should be using.
     * It should be followed by a call to XDGShellSurface::configure()
     * Once acked, it can relied upon to be applied in the next surface
     */
    void configure(Mode requestedMode);

    /**
     * The last mode requested by the client
     */
    Mode requestedMode() const;

    /**
     * The surface this decoration is attached to
     */
    XdgShellSurfaceInterface *surface() const;

Q_SIGNALS:
    /**
     * New mode requested by the client
     */
    void modeRequested(KWayland::Server::XdgDecorationInterface::Mode requestedMode);

private:
    explicit XdgDecorationInterface(XdgDecorationManagerInterface *parent, XdgShellSurfaceInterface *surface, wl_resource *parentResource);
    friend class XdgDecorationManagerInterface;

    class Private;
    Private *d_func() const;
};


}
}


#endif
