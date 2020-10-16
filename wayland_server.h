/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
class InputMethodV1Interface;
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
class XdgForeignV2Interface;
class XdgOutputManagerV1Interface;
class KeyStateInterface;
class LinuxDmabufUnstableV1Interface;
class LinuxDmabufUnstableV1Buffer;
class TabletManagerInterface;
class KeyboardShortcutsInhibitManagerV1Interface;
class XdgDecorationManagerV1Interface;
}


namespace KWin
{

class AbstractClient;
class Toplevel;
class XdgPopupClient;
class XdgSurfaceClient;
class XdgToplevelClient;
class AbstractWaylandOutput;

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
    bool start();
    void terminateClientConnections();

    KWaylandServer::Display *display() const
    {
        return m_display;
    }
    KWaylandServer::CompositorInterface *compositor() const
    {
        return m_compositor;
    }
    KWaylandServer::SeatInterface *seat() const
    {
        return m_seat;
    }
    KWaylandServer::TabletManagerInterface *tabletManager() const
    {
        return m_tabletManager;
    }
    KWaylandServer::DataDeviceManagerInterface *dataDeviceManager() const
    {
        return m_dataDeviceManager;
    }
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *virtualDesktopManagement() const
    {
        return m_virtualDesktopManagement;
    }
    KWaylandServer::PlasmaWindowManagementInterface *windowManagement() const
    {
        return m_windowManagement;
    }
    KWaylandServer::ServerSideDecorationManagerInterface *decorationManager() const {
        return m_decorationManager;
    }
    KWaylandServer::XdgOutputManagerV1Interface *xdgOutputManagerV1() const {
        return m_xdgOutputManagerV1;
    }
    KWaylandServer::KeyboardShortcutsInhibitManagerV1Interface *keyboardShortcutsInhibitManager() const
    {
        return m_keyboardShortcutsInhibitManager;
    }

    bool isKeyboardShortcutsInhibited() const;

    KWaylandServer::LinuxDmabufUnstableV1Interface *linuxDmabuf();

    KWaylandServer::InputMethodV1Interface *inputMethod() const {
        return m_inputMethod;
    }

    QList<AbstractClient *> clients() const {
        return m_clients;
    }
    void removeClient(AbstractClient *c);
    AbstractClient *findClient(quint32 id) const;
    AbstractClient *findClient(KWaylandServer::SurfaceInterface *surface) const;
    XdgToplevelClient *findXdgToplevelClient(KWaylandServer::SurfaceInterface *surface) const;
    XdgSurfaceClient *findXdgSurfaceClient(KWaylandServer::SurfaceInterface *surface) const;

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

    KWaylandServer::ClientConnection *xWaylandConnection() const;
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

    AbstractWaylandOutput *findOutput(KWaylandServer::OutputInterface *output) const;

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
    void initScreenLocker();
    void registerXdgGenericClient(AbstractClient *client);
    void registerXdgToplevelClient(XdgToplevelClient *client);
    void registerXdgPopupClient(XdgPopupClient *client);
    void registerShellClient(AbstractClient *client);
    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::CompositorInterface *m_compositor = nullptr;
    KWaylandServer::SeatInterface *m_seat = nullptr;
    KWaylandServer::TabletManagerInterface *m_tabletManager = nullptr;
    KWaylandServer::DataDeviceManagerInterface *m_dataDeviceManager = nullptr;
    KWaylandServer::PlasmaShellInterface *m_plasmaShell = nullptr;
    KWaylandServer::PlasmaWindowManagementInterface *m_windowManagement = nullptr;
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *m_virtualDesktopManagement = nullptr;
    KWaylandServer::ServerSideDecorationManagerInterface *m_decorationManager = nullptr;
    KWaylandServer::OutputManagementInterface *m_outputManagement = nullptr;
    KWaylandServer::AppMenuManagerInterface *m_appMenuManager = nullptr;
    KWaylandServer::ServerSideDecorationPaletteManagerInterface *m_paletteManager = nullptr;
    KWaylandServer::IdleInterface *m_idle = nullptr;
    KWaylandServer::XdgOutputManagerV1Interface *m_xdgOutputManagerV1 = nullptr;
    KWaylandServer::XdgDecorationManagerV1Interface *m_xdgDecorationManagerV1 = nullptr;
    KWaylandServer::LinuxDmabufUnstableV1Interface *m_linuxDmabuf = nullptr;
    KWaylandServer::KeyboardShortcutsInhibitManagerV1Interface *m_keyboardShortcutsInhibitManager = nullptr;
    QSet<KWaylandServer::LinuxDmabufUnstableV1Buffer*> m_linuxDmabufBuffers;
    QPointer<KWaylandServer::ClientConnection> m_xwaylandConnection;
    KWaylandServer::InputMethodV1Interface *m_inputMethod = nullptr;
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
        bool interfacesAnnounced = false;

    } m_internalConnection;
    KWaylandServer::XdgForeignV2Interface *m_XdgForeign = nullptr;
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

