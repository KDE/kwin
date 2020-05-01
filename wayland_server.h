/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_WAYLAND_SERVER_H
#define KWIN_WAYLAND_SERVER_H

#include <kwinglobals.h>
#include "keyboard_input.h"

#include <QObject>

class QThread;
class QProcess;
class QWindow;

namespace KWayland
{
namespace Client
{
class ConnectionThread;
class Registry;
class Compositor;
class Seat;
class DataDeviceManager;
class ShmPool;
class Surface;
}
}
namespace KWaylandServer
{
class AppMenuManagerInterface;
class ClientConnection;
class CompositorInterface;
class Display;
class DataDeviceInterface;
class IdleInterface;
class SeatInterface;
class DataDeviceManagerInterface;
class ServerSideDecorationManagerInterface;
class ServerSideDecorationPaletteManagerInterface;
class SurfaceInterface;
class OutputInterface;
class PlasmaShellInterface;
class PlasmaShellSurfaceInterface;
class PlasmaVirtualDesktopManagementInterface;
class PlasmaWindowManagementInterface;
class OutputManagementInterface;
class OutputConfigurationInterface;
class XdgDecorationManagerInterface;
class XdgShellInterface;
class XdgForeignInterface;
class XdgOutputManagerInterface;
class KeyStateInterface;
class LinuxDmabufUnstableV1Interface;
class LinuxDmabufUnstableV1Buffer;
class TabletManagerInterface;
}


namespace KWin
{
class XdgShellClient;

class AbstractClient;
class Toplevel;

class KWIN_EXPORT WaylandServer : public QObject
{
    Q_OBJECT
public:
    enum class InitializationFlag {
        NoOptions = 0x0,
        LockScreen = 0x1,
        NoLockScreenIntegration = 0x2,
        NoGlobalShortcuts = 0x4
    };

    Q_DECLARE_FLAGS(InitializationFlags, InitializationFlag)

    ~WaylandServer() override;
    bool init(const QByteArray &socketName = QByteArray(), InitializationFlags flags = InitializationFlag::NoOptions);
    void terminateClientConnections();

    KWaylandServer::Display *display() {
        return m_display;
    }
    KWaylandServer::CompositorInterface *compositor() {
        return m_compositor;
    }
    KWaylandServer::SeatInterface *seat() {
        return m_seat;
    }
    KWaylandServer::TabletManagerInterface *tabletManager()
    {
        return m_tabletManager;
    }
    KWaylandServer::DataDeviceManagerInterface *dataDeviceManager() {
        return m_dataDeviceManager;
    }
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *virtualDesktopManagement() {
        return m_virtualDesktopManagement;
    }
    KWaylandServer::PlasmaWindowManagementInterface *windowManagement() {
        return m_windowManagement;
    }
    KWaylandServer::ServerSideDecorationManagerInterface *decorationManager() const {
        return m_decorationManager;
    }
    KWaylandServer::XdgOutputManagerInterface *xdgOutputManager() const {
        return m_xdgOutputManager;
    }
    KWaylandServer::LinuxDmabufUnstableV1Interface *linuxDmabuf();

    QList<AbstractClient *> clients() const {
        return m_clients;
    }
    void removeClient(AbstractClient *c);
    AbstractClient *findClient(quint32 id) const;
    AbstractClient *findClient(KWaylandServer::SurfaceInterface *surface) const;
    XdgShellClient *findXdgShellClient(KWaylandServer::SurfaceInterface *surface) const;

    /**
     * @returns a transient parent of a surface imported with the foreign protocol, if any
     */
    KWaylandServer::SurfaceInterface *findForeignTransientForSurface(KWaylandServer::SurfaceInterface *surface);

    /**
     * @returns file descriptor for Xwayland to connect to.
     */
    int createXWaylandConnection();
    void destroyXWaylandConnection();

    /**
     * @returns file descriptor to the input method server's socket.
     */
    int createInputMethodConnection();
    void destroyInputMethodConnection();

    /**
     * @returns true if screen is locked.
     */
    bool isScreenLocked() const;
    /**
     * @returns whether integration with KScreenLocker is available.
     */
    bool hasScreenLockerIntegration() const;

    /**
     * @returns whether any kind of global shortcuts are supported.
     */
    bool hasGlobalShortcutSupport() const;

    void createInternalConnection();
    void initWorkspace();

    KWaylandServer::ClientConnection *xWaylandConnection() const {
        return m_xwayland.client;
    }
    KWaylandServer::ClientConnection *inputMethodConnection() const {
        return m_inputMethodServerConnection;
    }
    KWaylandServer::ClientConnection *internalConnection() const {
        return m_internalConnection.server;
    }
    KWaylandServer::ClientConnection *screenLockerClientConnection() const {
        return m_screenLockerClientConnection;
    }
    KWayland::Client::Compositor *internalCompositor() {
        return m_internalConnection.compositor;
    }
    KWayland::Client::Seat *internalSeat() {
        return m_internalConnection.seat;
    }
    KWayland::Client::DataDeviceManager *internalDataDeviceManager() {
        return m_internalConnection.ddm;
    }
    KWayland::Client::ShmPool *internalShmPool() {
        return m_internalConnection.shm;
    }
    KWayland::Client::ConnectionThread *internalClientConection() {
        return m_internalConnection.client;
    }
    KWayland::Client::Registry *internalClientRegistry() {
        return m_internalConnection.registry;
    }
    void dispatch();
    quint32 createWindowId(KWaylandServer::SurfaceInterface *surface);

    /**
     * Struct containing information for a created Wayland connection through a
     * socketpair.
     */
    struct SocketPairConnection {
        /**
         * ServerSide Connection
         */
        KWaylandServer::ClientConnection *connection = nullptr;
        /**
         * client-side file descriptor for the socket
         */
        int fd = -1;
    };
    /**
     * Creates a Wayland connection using a socket pair.
     */
    SocketPairConnection createConnection();

    void simulateUserActivity();
    void updateKeyState(KWin::Xkb::LEDs leds);

    QSet<KWaylandServer::LinuxDmabufUnstableV1Buffer*> linuxDmabufBuffers() const {
        return m_linuxDmabufBuffers;
    }
    void addLinuxDmabufBuffer(KWaylandServer::LinuxDmabufUnstableV1Buffer *buffer) {
        m_linuxDmabufBuffers << buffer;
    }
    void removeLinuxDmabufBuffer(KWaylandServer::LinuxDmabufUnstableV1Buffer *buffer) {
        m_linuxDmabufBuffers.remove(buffer);
    }

Q_SIGNALS:
    void shellClientAdded(KWin::AbstractClient *);
    void shellClientRemoved(KWin::AbstractClient *);
    void terminatingInternalClientConnection();
    void initialized();
    void foreignTransientChanged(KWaylandServer::SurfaceInterface *child);

private:
    int createScreenLockerConnection();
    void shellClientShown(Toplevel *t);
    quint16 createClientId(KWaylandServer::ClientConnection *c);
    void destroyInternalConnection();
    template <class T>
    void createSurface(T *surface);
    void initScreenLocker();
    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::CompositorInterface *m_compositor = nullptr;
    KWaylandServer::SeatInterface *m_seat = nullptr;
    KWaylandServer::TabletManagerInterface *m_tabletManager = nullptr;
    KWaylandServer::DataDeviceManagerInterface *m_dataDeviceManager = nullptr;
    KWaylandServer::XdgShellInterface *m_xdgShell = nullptr;
    KWaylandServer::PlasmaShellInterface *m_plasmaShell = nullptr;
    KWaylandServer::PlasmaWindowManagementInterface *m_windowManagement = nullptr;
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *m_virtualDesktopManagement = nullptr;
    KWaylandServer::ServerSideDecorationManagerInterface *m_decorationManager = nullptr;
    KWaylandServer::OutputManagementInterface *m_outputManagement = nullptr;
    KWaylandServer::AppMenuManagerInterface *m_appMenuManager = nullptr;
    KWaylandServer::ServerSideDecorationPaletteManagerInterface *m_paletteManager = nullptr;
    KWaylandServer::IdleInterface *m_idle = nullptr;
    KWaylandServer::XdgOutputManagerInterface *m_xdgOutputManager = nullptr;
    KWaylandServer::XdgDecorationManagerInterface *m_xdgDecorationManager = nullptr;
    KWaylandServer::LinuxDmabufUnstableV1Interface *m_linuxDmabuf = nullptr;
    QSet<KWaylandServer::LinuxDmabufUnstableV1Buffer*> m_linuxDmabufBuffers;
    struct {
        KWaylandServer::ClientConnection *client = nullptr;
        QMetaObject::Connection destroyConnection;
    } m_xwayland;
    KWaylandServer::ClientConnection *m_inputMethodServerConnection = nullptr;
    KWaylandServer::ClientConnection *m_screenLockerClientConnection = nullptr;
    struct {
        KWaylandServer::ClientConnection *server = nullptr;
        KWayland::Client::ConnectionThread *client = nullptr;
        QThread *clientThread = nullptr;
        KWayland::Client::Registry *registry = nullptr;
        KWayland::Client::Compositor *compositor = nullptr;
        KWayland::Client::Seat *seat = nullptr;
        KWayland::Client::DataDeviceManager *ddm = nullptr;
        KWayland::Client::ShmPool *shm = nullptr;
        bool interfacesAnnounced = false;

    } m_internalConnection;
    KWaylandServer::XdgForeignInterface *m_XdgForeign = nullptr;
    KWaylandServer::KeyStateInterface *m_keyState = nullptr;
    QList<AbstractClient *> m_clients;
    QHash<KWaylandServer::ClientConnection*, quint16> m_clientIds;
    InitializationFlags m_initFlags;
    QVector<KWaylandServer::PlasmaShellSurfaceInterface*> m_plasmaShellSurfaces;
    KWIN_SINGLETON(WaylandServer)
};

inline
WaylandServer *waylandServer() {
    return WaylandServer::self();
}

} // namespace KWin

#endif

