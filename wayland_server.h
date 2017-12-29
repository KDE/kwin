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

#include <QObject>
#include <QPointer>

class QThread;
class QProcess;
class QWindow;

namespace KWayland
{
namespace Client
{
class ConnectionThread;
class Registry;
class ShmPool;
class Surface;
}
namespace Server
{
class AppMenuManagerInterface;
class ClientConnection;
class CompositorInterface;
class Display;
class DataDeviceInterface;
class IdleInterface;
class ShellInterface;
class SeatInterface;
class ServerSideDecorationManagerInterface;
class ServerSideDecorationPaletteManagerInterface;
class SurfaceInterface;
class OutputInterface;
class PlasmaShellInterface;
class PlasmaShellSurfaceInterface;
class PlasmaWindowManagementInterface;
class QtSurfaceExtensionInterface;
class OutputManagementInterface;
class OutputConfigurationInterface;
class XdgShellInterface;
class XdgForeignInterface;
}
}

namespace KWin
{
class ShellClient;

class AbstractClient;
class Toplevel;

class KWIN_EXPORT WaylandServer : public QObject
{
    Q_OBJECT
public:
    enum class InitalizationFlag {
        NoOptions = 0x0,
        LockScreen = 0x1,
        NoLockScreenIntegration = 0x2
    };

    Q_DECLARE_FLAGS(InitalizationFlags, InitalizationFlag)

    virtual ~WaylandServer();
    bool init(const QByteArray &socketName = QByteArray(), InitalizationFlags flags = InitalizationFlag::NoOptions);
    void terminateClientConnections();

    KWayland::Server::Display *display() {
        return m_display;
    }
    KWayland::Server::CompositorInterface *compositor() {
        return m_compositor;
    }
    KWayland::Server::SeatInterface *seat() {
        return m_seat;
    }
    KWayland::Server::ShellInterface *shell() {
        return m_shell;
    }
    KWayland::Server::PlasmaWindowManagementInterface *windowManagement() {
        return m_windowManagement;
    }
    KWayland::Server::ServerSideDecorationManagerInterface *decorationManager() const {
        return m_decorationManager;
    }
    QList<ShellClient*> clients() const {
        return m_clients;
    }
    QList<ShellClient*> internalClients() const {
        return m_internalClients;
    }
    void removeClient(ShellClient *c);
    ShellClient *findClient(quint32 id) const;
    ShellClient *findClient(KWayland::Server::SurfaceInterface *surface) const;
    AbstractClient *findAbstractClient(KWayland::Server::SurfaceInterface *surface) const;
    ShellClient *findClient(QWindow *w) const;

    /**
     * return a transient parent of a surface imported with the foreign protocol, if any
     */
    KWayland::Server::SurfaceInterface *findForeignTransientForSurface(KWayland::Server::SurfaceInterface *surface);

    /**
     * @returns file descriptor for Xwayland to connect to.
     **/
    int createXWaylandConnection();
    void destroyXWaylandConnection();

    /**
     * @returns file descriptor to the input method server's socket.
     **/
    int createInputMethodConnection();
    void destroyInputMethodConnection();

    int createXclipboardSyncConnection();

    /**
     * @returns true if screen is locked.
     **/
    bool isScreenLocked() const;
    /**
     * @returns whether integration with KScreenLocker is available.
     **/
    bool hasScreenLockerIntegration() const;

    void createInternalConnection();
    void initWorkspace();

    KWayland::Server::ClientConnection *xWaylandConnection() const {
        return m_xwayland.client;
    }
    KWayland::Server::ClientConnection *inputMethodConnection() const {
        return m_inputMethodServerConnection;
    }
    KWayland::Server::ClientConnection *internalConnection() const {
        return m_internalConnection.server;
    }
    KWayland::Server::ClientConnection *screenLockerClientConnection() const {
        return m_screenLockerClientConnection;
    }
    QPointer<KWayland::Server::DataDeviceInterface> xclipboardSyncDataDevice() const {
        return m_xclipbaordSync.ddi;
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
    quint32 createWindowId(KWayland::Server::SurfaceInterface *surface);

    /**
     * Struct containing information for a created Wayland connection through a
     * socketpair.
     **/
    struct SocketPairConnection {
        /**
         * ServerSide Connection
         **/
        KWayland::Server::ClientConnection *connection = nullptr;
        /**
         * client-side file descriptor for the socket
         **/
        int fd = -1;
    };
    /**
     * Creates a Wayland connection using a socket pair.
     **/
    SocketPairConnection createConnection();

    void simulateUserActivity();

Q_SIGNALS:
    void shellClientAdded(KWin::ShellClient*);
    void shellClientRemoved(KWin::ShellClient*);
    void terminatingInternalClientConnection();
    void initialized();
    void foreignTransientChanged(KWayland::Server::SurfaceInterface *child);

private:
    void setupX11ClipboardSync();
    void shellClientShown(Toplevel *t);
    void initOutputs();
    void syncOutputsToWayland();
    quint16 createClientId(KWayland::Server::ClientConnection *c);
    void destroyInternalConnection();
    void configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config);
    template <class T>
    void createSurface(T *surface);
    void initScreenLocker();
    KWayland::Server::Display *m_display = nullptr;
    KWayland::Server::CompositorInterface *m_compositor = nullptr;
    KWayland::Server::SeatInterface *m_seat = nullptr;
    KWayland::Server::ShellInterface *m_shell = nullptr;
    KWayland::Server::XdgShellInterface *m_xdgShell = nullptr;
    KWayland::Server::XdgShellInterface *m_xdgShell6 = nullptr;
    KWayland::Server::PlasmaShellInterface *m_plasmaShell = nullptr;
    KWayland::Server::PlasmaWindowManagementInterface *m_windowManagement = nullptr;
    KWayland::Server::QtSurfaceExtensionInterface *m_qtExtendedSurface = nullptr;
    KWayland::Server::ServerSideDecorationManagerInterface *m_decorationManager = nullptr;
    KWayland::Server::OutputManagementInterface *m_outputManagement = nullptr;
    KWayland::Server::AppMenuManagerInterface *m_appMenuManager = nullptr;
    KWayland::Server::ServerSideDecorationPaletteManagerInterface *m_paletteManager = nullptr;
    KWayland::Server::IdleInterface *m_idle = nullptr;
    struct {
        KWayland::Server::ClientConnection *client = nullptr;
        QMetaObject::Connection destroyConnection;
    } m_xwayland;
    KWayland::Server::ClientConnection *m_inputMethodServerConnection = nullptr;
    KWayland::Server::ClientConnection *m_screenLockerClientConnection = nullptr;
    struct {
        KWayland::Server::ClientConnection *server = nullptr;
        KWayland::Client::ConnectionThread *client = nullptr;
        QThread *clientThread = nullptr;
        KWayland::Client::Registry *registry = nullptr;
        KWayland::Client::ShmPool *shm = nullptr;
        bool interfacesAnnounced = false;

    } m_internalConnection;
    struct {
        QProcess *process = nullptr;
        KWayland::Server::ClientConnection *client = nullptr;
        QPointer<KWayland::Server::DataDeviceInterface> ddi;
    } m_xclipbaordSync;
    KWayland::Server::XdgForeignInterface *m_XdgForeign = nullptr;
    QList<ShellClient*> m_clients;
    QList<ShellClient*> m_internalClients;
    QHash<KWayland::Server::ClientConnection*, quint16> m_clientIds;
    InitalizationFlags m_initFlags;
    QVector<KWayland::Server::PlasmaShellSurfaceInterface*> m_plasmaShellSurfaces;
    KWIN_SINGLETON(WaylandServer)
};

inline
WaylandServer *waylandServer() {
    return WaylandServer::self();
}

} // namespace KWin

#endif

