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
    UnstableV5
};

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
    XdgShellSurfaceInterface *getSurface(wl_resource *native);

Q_SIGNALS:
    void surfaceCreated(KWayland::Server::XdgShellSurfaceInterface *surface);
    /**
     * Emitted whenever a new popup got created.
     *
     * A popup only gets created in response to an action on the @p seat.
     *
     * @param surface The popup xdg shell surface which got created
     * @param seat The seat on which an action triggered the popup
     * @param serial The serial of the action on the seat
     **/
    void popupCreated(KWayland::Server::XdgShellPopupInterface *surface, KWayland::Server::SeatInterface *seat, quint32 serial);

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

    /**
     * @returns the parent surface.
     * @see transientOffset
     **/
    QPointer<SurfaceInterface> transientFor() const;
    /**
     * The offset of the Surface in the coordinate system of the SurfaceInterface this surface is a transient for.
     *
     * @returns offset in parent coordinate system.
     * @see transientFor
     **/
    QPoint transientOffset() const;

    /**
     * Dismiss this popup. This indicates to the client that it should destroy this popup.
     * The Compositor can invoke this method when e.g. the user clicked outside the popup
     * to dismiss it.
     **/
    void popupDone();

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

#endif
