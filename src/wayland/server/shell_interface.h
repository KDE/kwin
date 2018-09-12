/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
*********************************************************************/
#ifndef WAYLAND_SERVER_SHELL_INTERFACE_H
#define WAYLAND_SERVER_SHELL_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>
#include "seat_interface.h"

#include "global.h"
#include "resource.h"

class QSize;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class SeatInterface;
class SurfaceInterface;
class ShellSurfaceInterface;
template <typename T>
class GenericShellSurface;

/**
 * @brief Global for the wl_shell interface.
 *
 * @see ShellSurfaceInterface
 * @see SurfaceInterface
 **/
class KWAYLANDSERVER_EXPORT ShellInterface : public Global
{
    Q_OBJECT
public:
    virtual ~ShellInterface();

Q_SIGNALS:
    /**
     * Emitted whenever a new ShellSurfaceInterface gets created for a SurfaceInterface.
     **/
    void surfaceCreated(KWayland::Server::ShellSurfaceInterface*);

private:
    friend class Display;
    explicit ShellInterface(Display *display, QObject *parent);
    class Private;
};

/**
 * @brief Resource for a wl_shell_surface.
 *
 * The ShellSurfaceInterface represents a "normal window". It gets created for a
 * SurfaceInterface, thus has visible content. Through the ShellSurfaceInterface the
 * client can specify further meta-information about how the SurfaceInterface should be
 * represented.
 *
 * @see SurfaceInterface
 * @see ShellInterface
 **/
class KWAYLANDSERVER_EXPORT ShellSurfaceInterface : public Resource
{
    Q_OBJECT
    /**
     * The window title
     **/
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    /**
     * The window class, representing the desktop file name.
     **/
    Q_PROPERTY(QByteArray windowClass READ windowClass NOTIFY windowClassChanged)
    /**
     * Whether the window is fullscreen.
     **/
    Q_PROPERTY(bool fullscreen READ isFullscreen NOTIFY fullscreenChanged)
    /**
     * Whether the window is a normal toplevel window (not a child).
     **/
    Q_PROPERTY(bool toplevel READ isToplevel NOTIFY toplevelChanged)
    /**
     * Whether the window is maximized.
     **/
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    /**
     * Whether the ShellSurfaceInterface is a popup for another SurfaceInterface.
     *
     * Popup implies transient.
     * @since 5.5
     **/
    Q_PROPERTY(bool popup READ isPopup NOTIFY popupChanged)
    /**
     * Whether the ShellSurfaceInterface is a transient for another SurfaceInterface.
     *
     * Popup implies transient.
     * @since 5.5
     **/
    Q_PROPERTY(bool transient READ isTransient NOTIFY transientChanged)
    /**
     * Offset of the upper left corner in the parent SurfaceInterface's coordinate system.
     * @since 5.5
     **/
    Q_PROPERTY(QPoint transientOffset READ transientOffset NOTIFY transientOffsetChanged)
    /**
     * Whether the ShellSurfaceInterface can accept keyboard focus.
     *
     * By default ShellSurfaceInterface accepts keyboard focus, only transient surfaces
     * might not want keyboard focus.
     *
     * @since 5.5
     **/
    Q_PROPERTY(bool acceptsKeyboardFocus READ acceptsKeyboardFocus NOTIFY acceptsKeyboardFocusChanged)
public:
    virtual ~ShellSurfaceInterface();

    /**
     * Pings the client.
     * The client is required to send a pong. If that is not received in the times tamp
     * set through setPingTimeout the signal @link pingTimeout @endlink will be emitted.
     * If a pong is received the signal @link pongReceived @endlink will be emitted.
     *
     * @see setPingTimeout
     * @see pingTimeout
     * @see pongReceived
     * @see isPinged
     **/
    void ping();
    /**
     * Sets the ping time out for @link ping @endlink requests to @p msec.
     *
     * @param msec The time out in msec
     * @see ping
     * @see isPinged
     **/
    void setPingTimeout(uint msec);
    /**
     * @returns whether the ShellSurfaceInterface got pinged, but no pong received.
     * @see ping
     **/
    bool isPinged() const;
    /**
     * Requests that the ShellSurfaceInterface resizes the SurfaceInterface to @p size.
     **/
    void requestSize(const QSize &size);

    /**
     * @return The SurfaceInterface this ShellSurfaceInterface got created for.
     **/
    SurfaceInterface *surface() const;
    /**
     * @returns The ShellInterface which created this ShellSurfaceInterface.
     **/
    ShellInterface *shell() const;

    QString title() const;
    QByteArray windowClass() const;
    bool isFullscreen() const;
    bool isToplevel() const;
    bool isMaximized() const;
    /**
     * @returns @c true if the ShellSurfaceInterface is a popup.
     * @see isTransient
     * @see transientOffset
     * @see transientFor
     * @since 5.5
     **/
    bool isPopup() const;
    /**
     * @returns @c true if the ShellSurfaceInterface is a transient or popup for another SurfaceInterface.
     * @see isPopup
     * @see transientOffset
     * @see transientFor
     * @since 5.5
     **/
    bool isTransient() const;
    /**
     * In case the ShellSurfaceInterface is a transient this is the offset of the ShellSurfaceInterface
     * in the coordinate system of the SurfaceInterface this surface is a transient for.
     *
     * @returns offset in parent coordinate system.
     * @see isTransient
     * @see transientFor
     * @since 5.5
     **/
    QPoint transientOffset() const;
    /**
     * The SurfaceInterface for which this ShellSurfaceInterface is a transient.
     * This is only relevant if the ShellSurfaceInterface is either a transient or a
     * popup.
     *
     * The transientOffset is in the local coordinate system of the SurfaceInterface
     * returned by this method.
     *
     * @returns The SurfaceInterface for which this Surface is a transient
     * @see isTransient
     * @see isPopup
     * @see transientOffset
     * @since 5.5
     **/
    QPointer<SurfaceInterface> transientFor() const;

    /**
     * Whether the ShellSurfaceInterface can accept keyboard focus.
     *
     * This is only relevant for transient and popup windows. By default all ShellSurfaces
     * accept keyboard focus.
     *
     * @returns Whether the ShellSurfaceInterface can accept keyboard focus.
     * @see isTransient()
     * @see acceptsKeyboardFocusChanged
     * @since 5.5
     **/
    bool acceptsKeyboardFocus() const;

    /**
     * Sends a popup done event to the shell surface.
     * This is only relevant for popup windows. It indicates that the popup grab
     * got canceled. This happens when e.g. the user clicks outside of any surface
     * of the same client as this ShellSurfaceInterface. It is the task of the
     * compositor to send the popupDone event appropriately.
     *
     * @see isPopup
     * @since 5.33
     **/
    void popupDone();

Q_SIGNALS:
    /**
     * Emitted whenever the title changes.
     **/
    void titleChanged(const QString&);
    /**
     * Emitted whenever the window class changes.
     **/
    void windowClassChanged(const QByteArray&);
    /**
     * Emitted when the ping timed out.
     * @see ping
     * @see pingTimeout
     * @see isPinged
     **/
    void pingTimeout();
    /**
     * Emitted when the server received a pong for this ShellSurfaceInterface.
     **/
    void pongReceived();
    void fullscreenChanged(bool);
    void toplevelChanged(bool);
    void maximizedChanged(bool);
    /**
     * @since 5.5
     **/
    void popupChanged(bool);
    /**
     * @since 5.5
     **/
    void transientChanged(bool);
    /**
     * @since 5.5
     **/
    void transientOffsetChanged(const QPoint&);
    /**
     * @since 5.5
     **/
    void transientForChanged();
    /**
     * @since 5.5
     **/
    void acceptsKeyboardFocusChanged();
    /**
     * The surface requested a window move.
     *
     * @param seat The SeatInterface on which the surface requested the move
     * @param serial The serial of the implicit mouse grab which triggered the move
     * @since 5.5
     **/
    void moveRequested(KWayland::Server::SeatInterface *seat, quint32 serial);
    /**
     * The surface requested a window resize.
     *
     * @param seat The SeatInterface on which the surface requested the resize
     * @param serial The serial of the implicit mouse grab which triggered the resize
     * @param edges A hint which edges are involved in the resize
     * @since 5.5
     **/
    void resizeRequested(KWayland::Server::SeatInterface *seat, quint32 serial, Qt::Edges edges);

private:
    friend class ShellInterface;
    explicit ShellSurfaceInterface(ShellInterface *shell, SurfaceInterface *parent, wl_resource *parentResource);
    friend class GenericShellSurface<ShellSurfaceInterface>;
    class Private;
    Private *d_func() const;
};

}
}

#endif
