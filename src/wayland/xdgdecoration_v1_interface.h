/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class XdgDecorationManagerV1InterfacePrivate;
class XdgToplevelDecorationV1Interface;
class XdgToplevelDecorationV1InterfacePrivate;
class XdgToplevelInterface;

/**
 * The XdgDecorationManagerV1Interface class provides a way for the compositor and an xdg-shell
 * client to negotiate the use of server-side window decorations.
 *
 * XdgDecorationManagerV1Interface corresponds to the interface \c zxdg_decoration_manager_v1.
 *
 * \since 5.20
 */
class KWAYLANDSERVER_EXPORT XdgDecorationManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a decoration manager with the given \a display and \a parent.
     */
    XdgDecorationManagerV1Interface(Display *display, QObject *parent = nullptr);
    /**
     * Destructs the XdgDecorationManagerV1Interface object.
     */
    ~XdgDecorationManagerV1Interface() override;

Q_SIGNALS:
    /**
     * This signal is emitted when a new \a decoration has been created.
     */
    void decorationCreated(XdgToplevelDecorationV1Interface *decoration);

private:
    QScopedPointer<XdgDecorationManagerV1InterfacePrivate> d;
};

/**
 * The XdgToplevelDecorationV1Interface class allows the compositor to toggle server-side window
 * decoration on an xdg-toplevel surface.
 *
 * XdgToplevelDecorationV1Interface corresponds to the interface \c zxdg_toplevel_decoration_v1.
 *
 * \since 5.20
 */
class KWAYLANDSERVER_EXPORT XdgToplevelDecorationV1Interface : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Undefined, Client, Server };
    Q_ENUM(Mode)

    /**
     * Constructs a XdgToplevelDecorationV1Interface for the given \a toplevel and initializes
     * it with \a resource.
     */
    XdgToplevelDecorationV1Interface(XdgToplevelInterface *toplevel, ::wl_resource *resource);
    /**
     * Destructs the XdgToplevelDecorationV1Interface object.
     */
    ~XdgToplevelDecorationV1Interface() override;

    /**
     * Returns the toplevel for this XdgToplevelDecorationV1Interface.
     */
    XdgToplevelInterface *toplevel() const;
    /**
     * Returns the decoration mode preferred by the client.
     */
    Mode preferredMode() const;
    /**
     * Sends a configure event to the client. \a mode indicates the decoration mode the client
     * should be using. The client must send an ack_configure in response to this event.
     *
     * \see XdgToplevelInterface::sendConfigure
     */
    void sendConfigure(Mode mode);

    /**
     * Returns the XdgToplevelDecorationV1Interface for the specified \a toplevel.
     */
    static XdgToplevelDecorationV1Interface *get(XdgToplevelInterface *toplevel);

Q_SIGNALS:
    /**
     * This signal is emitted when the client has specified the preferred decoration mode. The
     * compositor can decide not to use the client's mode and enforce a different mode instead.
     */
    void preferredModeChanged(KWaylandServer::XdgToplevelDecorationV1Interface::Mode mode);

private:
    QScopedPointer<XdgToplevelDecorationV1InterfacePrivate> d;
};

} // namespace KWaylandServer
