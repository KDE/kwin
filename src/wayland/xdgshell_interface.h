/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWAYLAND_SERVER_XDGSHELL_INTERFACE_H
#define KWAYLAND_SERVER_XDGSHELL_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <QSize>

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class OutputInterface;
class SeatInterface;
class SurfaceInterface;
class XdgShellPopupInterface;
class XdgShellSurfaceInterface;
template <typename T>
class GenericShellSurface;

/**
 * Enum describing the different InterfaceVersion encapsulated in this implementation.
 *
 * @since 5.25
 **/
enum class XdgShellInterfaceVersion
{
    /**
     * xdg_shell (unstable v5)
     **/
    UnstableV5,
    /**
     * zxdg_shell_v6 (unstable v6)
     * @since 5.39
     **/
    UnstableV6,
    /**
      xdg_wm_base (stable)
      @since 5.48
      */
    Stable
};

/**
 * Flags describing how a popup should be reposition if constrained
 * @since 5.39
 */
enum class PositionerConstraint {
    /**
     * Slide the popup on the X axis until there is room
     */
    SlideX = 1 << 0,
    /**
     * Slide the popup on the Y axis until there is room
     */
    SlideY = 1 << 1,
    /**
     * Invert the anchor and gravity on the X axis
     */
    FlipX = 1 << 2,
    /**
     * Invert the anchor and gravity on the Y axis
     */
    FlipY = 1 << 3,
    /**
     * Resize the popup in the X axis
     */
    ResizeX = 1 << 4,
    /**
     * Resize the popup in the Y axis
     */
    ResizeY = 1 << 5
};

Q_DECLARE_FLAGS(PositionerConstraints, PositionerConstraint)

/**
 *
 * @since 5.25
 **/
class KWAYLANDSERVER_EXPORT XdgShellInterface : public Global
{
    Q_OBJECT
public:
    virtual ~XdgShellInterface();

    /**
     * @returns The interface version used by this XdgShellInterface
     **/
    XdgShellInterfaceVersion interfaceVersion() const;

    /**
     * @returns The XdgShellSurfaceInterface for the @p native resource.
     **/
    //TODO KF6 make virtual
    XdgShellSurfaceInterface *getSurface(wl_resource *native);

    /**
     * Confirm the client is still alive and responding
     *
     * Will result in pong being emitted
     *
     * @returns unique identifier for this request
     * @since 5.39
     */
    quint32 ping(XdgShellSurfaceInterface * surface);

Q_SIGNALS:
    void surfaceCreated(KWayland::Server::XdgShellSurfaceInterface *surface);

    /**
     * Emitted whenever a new popup got created.
     *
     * A popup only gets created in response to an action on the @p seat.
     *
     *
     * @param surface The popup xdg shell surface which got created
     * @param seat The seat on which an action triggered the popup
     * @param serial The serial of the action on the seat
     *
     * XDGV5 only
     * Use both xdgPopupCreated and XdgShellPopupInterface::grabbed to cover both XDGV5 and XDGV6
     **/

    void popupCreated(KWayland::Server::XdgShellPopupInterface *surface, KWayland::Server::SeatInterface *seat, quint32 serial);

    /*
     * Emitted whenever a new popup gets created.
     *
     * @param surface The popup xdg shell surface which got created
     * @since 5.39
     */
    void xdgPopupCreated(KWayland::Server::XdgShellPopupInterface *surface);

    /*
     * Emitted in response to a ping request
     *
     * @param serial unique identifier for the request
     * @since 5.39
     */
    void pongReceived(quint32 serial);

    /*
     * Emitted when the application takes more than expected
     * to answer to a ping, this will always be emitted before
     * eventuallt pingTimeout gets emitted
     *
     * @param serial unique identifier for the request
     * @since 5.39
     */
    void pingDelayed(quint32 serial);

    /*
     * Emitted when the application doesn't answer to a ping
     * and the serve gave up on it
     *
     * @param serial unique identifier for the request
     * @since 5.39
     */
    void pingTimeout(quint32 serial);

protected:
    class Private;
    explicit XdgShellInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};


/**
 *
 * @since 5.25
 **/
class KWAYLANDSERVER_EXPORT XdgShellSurfaceInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~XdgShellSurfaceInterface();

    /**
     * @returns The interface version used by this XdgShellSurfaceInterface
     **/
    XdgShellInterfaceVersion interfaceVersion() const;

    /**
     * States the Surface can be in
     **/
    enum class State {
        /**
         * The Surface is maximized.
         **/
        Maximized  = 1 << 0,
        /**
         * The Surface is fullscreen.
         **/
        Fullscreen = 1 << 1,
        /**
         * The Surface is currently being resized by the Compositor.
         **/
        Resizing   = 1 << 2,
        /**
         * The Surface is considered active. Does not imply keyboard focus.
         **/
        Activated  = 1 << 3
    };
    Q_DECLARE_FLAGS(States, State)

    /**
     * Sends a configure event to the Surface.
     * This tells the Surface the current @p states it is in and the @p size it should have.
     * If @p size has width and height at @c 0, the Surface can choose the size.
     *
     * The Surface acknowledges the configure event with {@link configureAcknowledged}.
     *
     * @param states The states the surface is in
     * @param size The requested size
     * @returns The serial of the configure event
     * @see configureAcknowledged
     * @see isConfigurePending
     **/
    quint32 configure(States states, const QSize &size = QSize(0, 0));

    /**
     * @returns @c true if there is a not yet acknowledged configure event.
     * @see configure
     * @see configureAcknowledged
     **/
    bool isConfigurePending() const;

    /**
     * @return The SurfaceInterface this XdgSurfaceV5Interface got created for.
     **/
    SurfaceInterface *surface() const;

    /**
     * @returns The title of this surface.
     * @see titleChanged
     **/
    QString title() const;
    QByteArray windowClass() const;

    /**
     * @returns Whether this Surface is a transient for another Surface, that is it has a parent.
     * @see transientFor
     **/
    bool isTransient() const;
    /**
     * @returns the parent surface if the surface is a transient for another surface
     * @see isTransient
     **/
    QPointer<XdgShellSurfaceInterface> transientFor() const;

    /**
     * Request the client to close the window.
     **/
    void close();

Q_SIGNALS:
    /**
     * Emitted whenever the title changes.
     *
     * @see title
     **/
    void titleChanged(const QString&);
    /**
     * Emitted whenever the window class changes.
     *
     * @see windowClass
     **/
    void windowClassChanged(const QByteArray&);
    /**
     * The surface requested a window move.
     *
     * @param seat The SeatInterface on which the surface requested the move
     * @param serial The serial of the implicit mouse grab which triggered the move
     **/
    void moveRequested(KWayland::Server::SeatInterface *seat, quint32 serial);
    /**
     * The surface requested a window resize.
     *
     * @param seat The SeatInterface on which the surface requested the resize
     * @param serial The serial of the implicit mouse grab which triggered the resize
     * @param edges A hint which edges are involved in the resize
     **/
    void resizeRequested(KWayland::Server::SeatInterface *seat, quint32 serial, Qt::Edges edges);
    void windowMenuRequested(KWayland::Server::SeatInterface *seat, quint32 serial, const QPoint &surfacePos);
    /**
     * The surface requested a change of maximized state.
     * @param maximized Whether the window wants to be maximized
     **/
    void maximizedChanged(bool maximized);
    /**
     * The surface requested a change of fullscreen state
     * @param fullscreen Whether the window wants to be fullscreen
     * @param output An optional output hint on which the window wants to be fullscreen
     **/
    void fullscreenChanged(bool fullscreen, KWayland::Server::OutputInterface *output);
    /**
     * The surface requested to be minimized.
     **/
    void minimizeRequested();
    /**
     * A configure event with @p serial got acknowledged.
     * @see configure
     **/
    void configureAcknowledged(quint32 serial);
    /**
     * Emitted whenever the parent surface changes.
     * @see isTransient
     * @see transientFor
     **/
    void transientForChanged();

    /**
     * Emitted whenever the maximum size hint changes
     * @since 5.39
     */
    void maxSizeChanged(const QSize &size);

    /**
     * Emitted whenever the minimum size hint changes
     * @since 5.39
     */
    void minSizeChanged(const QSize &size);

protected:
    class Private;
    explicit XdgShellSurfaceInterface(Private *p);

private:
    Private *d_func() const;
    friend class GenericShellSurface<XdgShellSurfaceInterface>;
};

/**
 *
 * @since 5.25
 **/
class KWAYLANDSERVER_EXPORT XdgShellPopupInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~XdgShellPopupInterface();

    /**
     * @return The SurfaceInterface this XdgShellPopupInterface got created for.
     **/
    SurfaceInterface *surface() const;

    /*
     * Ask the popup surface to configure itself for the given configuration.
     *
     * @arg rect. The position of the surface relative to the transient parent
     * @since 5.39
     */
    quint32 configure(const QRect &rect);

    /**
     * @returns the parent surface.
     * @see transientOffset
     **/
    QPointer<SurfaceInterface> transientFor() const;

    /**
     * The offset of the Surface in the coordinate system of the SurfaceInterface this surface is a transient for.
     *
     * For XDG V6 this returns the point on the anchorRect defined by the anchor edge.
     *
     * @returns offset in parent coordinate system.
     * @see transientFor
     **/
    QPoint transientOffset() const;

    /**
     * The size of the surface that is to be positioned.
     *
     * @since 5.39
     */
    QSize initialSize() const;

    /**
     * The area this popup should be positioned around
     * @since 5.39
     */
    QRect anchorRect() const;

    /**
     * Which edge of the anchor should the popup be positioned around
     * @since 5.39
     */
    Qt::Edges anchorEdge() const;

    /**
     * An additional offset that should be applied to the popup from the anchor rect
     *
     * @since 5.39
     */
    QPoint anchorOffset() const;

    /**
     * Specifies in what direction the popup should be positioned around the anchor
     * i.e if the gravity is "bottom", then then the top of top of the popup will be at the anchor edge
     * if the gravity is top, then the bottom of the popup will be at the anchor edge
     *
     * @since 5.39
     */

    //DAVE left + right is illegal, so this is possible a useless return value? Maybe an enum with 9 entries left, topleft, top, ..
    Qt::Edges gravity() const;

    /**
     * Specifies how the compositor should position the popup if it does not fit in the requested position
     * @since 5.39
     */
    PositionerConstraints constraintAdjustments() const;

    /**
     * Dismiss this popup. This indicates to the client that it should destroy this popup.
     * The Compositor can invoke this method when e.g. the user clicked outside the popup
     * to dismiss it.
     **/
    void popupDone();

Q_SIGNALS:
    /**
     * A configure event with @p serial got acknowledged.
     * Note: XdgV6 only
     * @see configure
     * @since 5.39
     **/
    void configureAcknowledged(quint32 serial);

    /**
     * The client requested that this popup takes an explicit grab
     *
     * @param seat The seat on which an action triggered the popup
     * @param serial The serial of the action on the seat
     * @since 5.39
     */
    void grabRequested(KWayland::Server::SeatInterface *seat, quint32 serial);

protected:
    class Private;
    explicit XdgShellPopupInterface(Private *p);

private:
    friend class GenericShellSurface<XdgShellPopupInterface>;

    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::XdgShellSurfaceInterface *)
Q_DECLARE_METATYPE(KWayland::Server::XdgShellPopupInterface *)
Q_DECLARE_METATYPE(KWayland::Server::XdgShellSurfaceInterface::State)
Q_DECLARE_METATYPE(KWayland::Server::XdgShellSurfaceInterface::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWayland::Server::PositionerConstraints)

#endif
