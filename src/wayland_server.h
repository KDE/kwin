/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwinglobals.h>

#include <QObject>
#include <QPointer>
#include <QSet>

class QThread;
class QProcess;
class QWindow;

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
class PlasmaWindowActivationFeedbackInterface;
class PlasmaVirtualDesktopManagementInterface;
class PlasmaWindowManagementInterface;
class OutputDeviceV2Interface;
class OutputManagementV2Interface;
class XdgForeignV2Interface;
class XdgOutputManagerV1Interface;
class LinuxDmaBufV1ClientBufferIntegration;
class LinuxDmaBufV1ClientBuffer;
class TabletManagerV2Interface;
class KeyboardShortcutsInhibitManagerV1Interface;
class XdgDecorationManagerV1Interface;
class XWaylandKeyboardGrabManagerV1Interface;
class ContentTypeManagerV1Interface;
class DrmLeaseManagerV1;
class TearingControlManagerV1Interface;
class XwaylandShellV1Interface;
class OutputOrderV1Interface;
}

namespace KWin
{

class Window;
class Output;
class XdgActivationV1Integration;
class XdgPopupWindow;
class XdgSurfaceWindow;
class XdgToplevelWindow;

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
    bool init(const QString &socketName, InitializationFlags flags = InitializationFlag::NoOptions);
    bool init(InitializationFlags flags = InitializationFlag::NoOptions);

    bool start();

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
    KWaylandServer::TabletManagerV2Interface *tabletManagerV2() const
    {
        return m_tabletManagerV2;
    }
    KWaylandServer::DataDeviceManagerInterface *dataDeviceManager() const
    {
        return m_dataDeviceManager;
    }
    KWaylandServer::PlasmaWindowActivationFeedbackInterface *plasmaActivationFeedback() const
    {
        return m_plasmaActivationFeedback;
    }
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *virtualDesktopManagement() const
    {
        return m_virtualDesktopManagement;
    }
    KWaylandServer::PlasmaWindowManagementInterface *windowManagement() const
    {
        return m_windowManagement;
    }
    KWaylandServer::ServerSideDecorationManagerInterface *decorationManager() const
    {
        return m_decorationManager;
    }
    KWaylandServer::XdgOutputManagerV1Interface *xdgOutputManagerV1() const
    {
        return m_xdgOutputManagerV1;
    }
    KWaylandServer::KeyboardShortcutsInhibitManagerV1Interface *keyboardShortcutsInhibitManager() const
    {
        return m_keyboardShortcutsInhibitManager;
    }
    KWaylandServer::XwaylandShellV1Interface *xwaylandShell() const
    {
        return m_xwaylandShell;
    }

    bool isKeyboardShortcutsInhibited() const;

    KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *linuxDmabuf();

    KWaylandServer::InputMethodV1Interface *inputMethod() const
    {
        return m_inputMethod;
    }
    KWaylandServer::IdleInterface *idle() const
    {
        return m_idle;
    }
    QList<Window *> windows() const
    {
        return m_windows;
    }
    void removeWindow(Window *c);
    Window *findWindow(const KWaylandServer::SurfaceInterface *surface) const;
    XdgToplevelWindow *findXdgToplevelWindow(KWaylandServer::SurfaceInterface *surface) const;
    XdgSurfaceWindow *findXdgSurfaceWindow(KWaylandServer::SurfaceInterface *surface) const;

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

    void initWorkspace();

    KWaylandServer::ClientConnection *xWaylandConnection() const;
    KWaylandServer::ClientConnection *inputMethodConnection() const;
    KWaylandServer::ClientConnection *screenLockerClientConnection() const
    {
        return m_screenLockerClientConnection;
    }

    /**
     * Struct containing information for a created Wayland connection through a
     * socketpair.
     */
    struct SocketPairConnection
    {
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

    QSet<KWaylandServer::LinuxDmaBufV1ClientBuffer *> linuxDmabufBuffers() const
    {
        return m_linuxDmabufBuffers;
    }
    void addLinuxDmabufBuffer(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer)
    {
        m_linuxDmabufBuffers << buffer;
    }
    void removeLinuxDmabufBuffer(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer)
    {
        m_linuxDmabufBuffers.remove(buffer);
    }

    /**
     * Returns the first socket name that can be used to connect to this server.
     * For a full list, use display()->socketNames()
     */
    QString socketName() const;

    XdgActivationV1Integration *xdgActivationIntegration() const
    {
        return m_xdgActivationIntegration;
    }

Q_SIGNALS:
    void windowAdded(KWin::Window *);
    void windowRemoved(KWin::Window *);
    void initialized();
    void foreignTransientChanged(KWaylandServer::SurfaceInterface *child);
    void lockStateChanged();

private:
    int createScreenLockerConnection();
    void windowShown(Window *t);
    void initScreenLocker();
    void registerXdgGenericWindow(Window *window);
    void registerXdgToplevelWindow(XdgToplevelWindow *window);
    void registerXdgPopupWindow(XdgPopupWindow *window);
    void registerWindow(Window *window);
    void handleOutputAdded(Output *output);
    void handleOutputRemoved(Output *output);
    void handleOutputEnabled(Output *output);
    void handleOutputDisabled(Output *output);

    class LockScreenPresentationWatcher : public QObject
    {
    public:
        LockScreenPresentationWatcher(WaylandServer *server);

    private:
        QSet<Output *> m_signaledOutputs;
    };

    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::CompositorInterface *m_compositor = nullptr;
    KWaylandServer::SeatInterface *m_seat = nullptr;
    KWaylandServer::TabletManagerV2Interface *m_tabletManagerV2 = nullptr;
    KWaylandServer::DataDeviceManagerInterface *m_dataDeviceManager = nullptr;
    KWaylandServer::PlasmaShellInterface *m_plasmaShell = nullptr;
    KWaylandServer::PlasmaWindowActivationFeedbackInterface *m_plasmaActivationFeedback = nullptr;
    KWaylandServer::PlasmaWindowManagementInterface *m_windowManagement = nullptr;
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *m_virtualDesktopManagement = nullptr;
    KWaylandServer::ServerSideDecorationManagerInterface *m_decorationManager = nullptr;
    KWaylandServer::OutputManagementV2Interface *m_outputManagement = nullptr;
    KWaylandServer::AppMenuManagerInterface *m_appMenuManager = nullptr;
    KWaylandServer::ServerSideDecorationPaletteManagerInterface *m_paletteManager = nullptr;
    KWaylandServer::IdleInterface *m_idle = nullptr;
    KWaylandServer::XdgOutputManagerV1Interface *m_xdgOutputManagerV1 = nullptr;
    KWaylandServer::XdgDecorationManagerV1Interface *m_xdgDecorationManagerV1 = nullptr;
    KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *m_linuxDmabuf = nullptr;
    KWaylandServer::KeyboardShortcutsInhibitManagerV1Interface *m_keyboardShortcutsInhibitManager = nullptr;
    QSet<KWaylandServer::LinuxDmaBufV1ClientBuffer *> m_linuxDmabufBuffers;
    QPointer<KWaylandServer::ClientConnection> m_xwaylandConnection;
    KWaylandServer::InputMethodV1Interface *m_inputMethod = nullptr;
    QPointer<KWaylandServer::ClientConnection> m_inputMethodServerConnection;
    KWaylandServer::ClientConnection *m_screenLockerClientConnection = nullptr;
    KWaylandServer::XdgForeignV2Interface *m_XdgForeign = nullptr;
    XdgActivationV1Integration *m_xdgActivationIntegration = nullptr;
    KWaylandServer::XWaylandKeyboardGrabManagerV1Interface *m_xWaylandKeyboardGrabManager = nullptr;
    KWaylandServer::ContentTypeManagerV1Interface *m_contentTypeManager = nullptr;
    KWaylandServer::TearingControlManagerV1Interface *m_tearingControlInterface = nullptr;
    KWaylandServer::XwaylandShellV1Interface *m_xwaylandShell = nullptr;
    QList<Window *> m_windows;
    InitializationFlags m_initFlags;
    QHash<Output *, KWaylandServer::OutputInterface *> m_waylandOutputs;
    QHash<Output *, KWaylandServer::OutputDeviceV2Interface *> m_waylandOutputDevices;
    KWaylandServer::DrmLeaseManagerV1 *m_leaseManager = nullptr;
    KWaylandServer::OutputOrderV1Interface *m_outputOrder = nullptr;
    KWIN_SINGLETON(WaylandServer)
};

inline WaylandServer *waylandServer()
{
    return WaylandServer::self();
}

} // namespace KWin
