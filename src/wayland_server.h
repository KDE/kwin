/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"

#include <QObject>
#include <QPointer>
#include <QSet>

namespace KWin
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
class DrmClientBufferIntegration;
class LinuxDmaBufV1ClientBufferIntegration;
class TabletManagerV2Interface;
class KeyboardShortcutsInhibitManagerV1Interface;
class XdgDecorationManagerV1Interface;
class XWaylandKeyboardGrabManagerV1Interface;
class ContentTypeManagerV1Interface;
class DrmLeaseManagerV1;
class TearingControlManagerV1Interface;
class XwaylandShellV1Interface;
class OutputOrderV1Interface;
class Window;
class Output;
class XdgActivationV1Integration;
class XdgPopupWindow;
class XdgSurfaceWindow;
class XdgToplevelWindow;
class PresentationTime;

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

    Display *display() const
    {
        return m_display;
    }
    CompositorInterface *compositor() const
    {
        return m_compositor;
    }
    SeatInterface *seat() const
    {
        return m_seat;
    }
    TabletManagerV2Interface *tabletManagerV2() const
    {
        return m_tabletManagerV2;
    }
    DataDeviceManagerInterface *dataDeviceManager() const
    {
        return m_dataDeviceManager;
    }
    PlasmaWindowActivationFeedbackInterface *plasmaActivationFeedback() const
    {
        return m_plasmaActivationFeedback;
    }
    PlasmaVirtualDesktopManagementInterface *virtualDesktopManagement() const
    {
        return m_virtualDesktopManagement;
    }
    PlasmaWindowManagementInterface *windowManagement() const
    {
        return m_windowManagement;
    }
    ServerSideDecorationManagerInterface *decorationManager() const
    {
        return m_decorationManager;
    }
    XdgOutputManagerV1Interface *xdgOutputManagerV1() const
    {
        return m_xdgOutputManagerV1;
    }
    KeyboardShortcutsInhibitManagerV1Interface *keyboardShortcutsInhibitManager() const
    {
        return m_keyboardShortcutsInhibitManager;
    }
    XwaylandShellV1Interface *xwaylandShell() const
    {
        return m_xwaylandShell;
    }

    bool isKeyboardShortcutsInhibited() const;

    DrmClientBufferIntegration *drm();
    LinuxDmaBufV1ClientBufferIntegration *linuxDmabuf();

    InputMethodV1Interface *inputMethod() const
    {
        return m_inputMethod;
    }
    IdleInterface *idle() const
    {
        return m_idle;
    }
    QList<Window *> windows() const
    {
        return m_windows;
    }
    void removeWindow(Window *c);
    Window *findWindow(const SurfaceInterface *surface) const;
    XdgToplevelWindow *findXdgToplevelWindow(SurfaceInterface *surface) const;
    XdgSurfaceWindow *findXdgSurfaceWindow(SurfaceInterface *surface) const;

    /**
     * @returns a transient parent of a surface imported with the foreign protocol, if any
     */
    SurfaceInterface *findForeignTransientForSurface(SurfaceInterface *surface);

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

    ClientConnection *xWaylandConnection() const;
    ClientConnection *inputMethodConnection() const;
    ClientConnection *screenLockerClientConnection() const
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
        ClientConnection *connection = nullptr;
        /**
         * client-side file descriptor for the socket
         */
        int fd = -1;
    };
    /**
     * Creates a Wayland connection using a socket pair.
     */
    SocketPairConnection createConnection();

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
    void foreignTransientChanged(KWin::SurfaceInterface *child);
    void lockStateChanged();

private:
    int createScreenLockerConnection();
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

    Display *m_display = nullptr;
    CompositorInterface *m_compositor = nullptr;
    SeatInterface *m_seat = nullptr;
    TabletManagerV2Interface *m_tabletManagerV2 = nullptr;
    DataDeviceManagerInterface *m_dataDeviceManager = nullptr;
    PlasmaShellInterface *m_plasmaShell = nullptr;
    PlasmaWindowActivationFeedbackInterface *m_plasmaActivationFeedback = nullptr;
    PlasmaWindowManagementInterface *m_windowManagement = nullptr;
    PlasmaVirtualDesktopManagementInterface *m_virtualDesktopManagement = nullptr;
    ServerSideDecorationManagerInterface *m_decorationManager = nullptr;
    OutputManagementV2Interface *m_outputManagement = nullptr;
    AppMenuManagerInterface *m_appMenuManager = nullptr;
    ServerSideDecorationPaletteManagerInterface *m_paletteManager = nullptr;
    IdleInterface *m_idle = nullptr;
    XdgOutputManagerV1Interface *m_xdgOutputManagerV1 = nullptr;
    XdgDecorationManagerV1Interface *m_xdgDecorationManagerV1 = nullptr;
    DrmClientBufferIntegration *m_drm = nullptr;
    LinuxDmaBufV1ClientBufferIntegration *m_linuxDmabuf = nullptr;
    KeyboardShortcutsInhibitManagerV1Interface *m_keyboardShortcutsInhibitManager = nullptr;
    QPointer<ClientConnection> m_xwaylandConnection;
    InputMethodV1Interface *m_inputMethod = nullptr;
    QPointer<ClientConnection> m_inputMethodServerConnection;
    ClientConnection *m_screenLockerClientConnection = nullptr;
    XdgForeignV2Interface *m_XdgForeign = nullptr;
    XdgActivationV1Integration *m_xdgActivationIntegration = nullptr;
    XWaylandKeyboardGrabManagerV1Interface *m_xWaylandKeyboardGrabManager = nullptr;
    ContentTypeManagerV1Interface *m_contentTypeManager = nullptr;
    TearingControlManagerV1Interface *m_tearingControlInterface = nullptr;
    XwaylandShellV1Interface *m_xwaylandShell = nullptr;
    PresentationTime *m_presentationTime = nullptr;
    QList<Window *> m_windows;
    InitializationFlags m_initFlags;
    QHash<Output *, OutputInterface *> m_waylandOutputs;
    QHash<Output *, OutputDeviceV2Interface *> m_waylandOutputDevices;
    DrmLeaseManagerV1 *m_leaseManager = nullptr;
    OutputOrderV1Interface *m_outputOrder = nullptr;
    KWIN_SINGLETON(WaylandServer)
};

inline WaylandServer *waylandServer()
{
    return WaylandServer::self();
}

} // namespace KWin
