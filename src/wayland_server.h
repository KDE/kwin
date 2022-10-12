/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_WAYLAND_SERVER_H
#define KWIN_WAYLAND_SERVER_H

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
class PrimaryOutputV1Interface;
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
class ViewporterInterface;
class PointerGesturesV1Interface;
class PointerConstraintsV1Interface;
class RelativePointerManagerV1Interface;
class DataControlDeviceManagerV1Interface;
class PrimarySelectionDeviceManagerV1Interface;
class IdleInhibitManagerV1Interface;
class ShadowManagerInterface;
class DpmsManagerInterface;
class SubCompositorInterface;
class XdgActivationV1Interface;
class KeyStateInterface;
class LockscreenOverlayV1Interface;
}

namespace KWin
{

class Window;
class Output;
class XdgActivationV1Integration;
class XdgPopupWindow;
class XdgSurfaceWindow;
class XdgToplevelWindow;
class WaylandOutput;
class InputPanelV1Integration;
class XdgShellIntegration;
class LayerShellV1Integration;
class IdleInhibition;

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
        return m_display.get();
    }
    KWaylandServer::CompositorInterface *compositor() const
    {
        return m_compositor.get();
    }
    KWaylandServer::SeatInterface *seat() const
    {
        return m_seat.get();
    }
    KWaylandServer::TabletManagerV2Interface *tabletManagerV2() const
    {
        return m_tabletManagerV2.get();
    }
    KWaylandServer::DataDeviceManagerInterface *dataDeviceManager() const
    {
        return m_dataDeviceManager.get();
    }
    KWaylandServer::PlasmaWindowActivationFeedbackInterface *plasmaActivationFeedback() const
    {
        return m_plasmaActivationFeedback.get();
    }
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *virtualDesktopManagement() const
    {
        return m_virtualDesktopManagement.get();
    }
    KWaylandServer::PlasmaWindowManagementInterface *windowManagement() const
    {
        return m_windowManagement.get();
    }
    KWaylandServer::ServerSideDecorationManagerInterface *decorationManager() const
    {
        return m_decorationManager.get();
    }
    KWaylandServer::XdgOutputManagerV1Interface *xdgOutputManagerV1() const
    {
        return m_xdgOutputManagerV1.get();
    }
    KWaylandServer::KeyboardShortcutsInhibitManagerV1Interface *keyboardShortcutsInhibitManager() const
    {
        return m_keyboardShortcutsInhibitManager.get();
    }

    bool isKeyboardShortcutsInhibited() const;

    KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *linuxDmabuf();

    KWaylandServer::InputMethodV1Interface *inputMethod() const
    {
        return m_inputMethod.get();
    }
    KWaylandServer::IdleInterface *idle() const
    {
        return m_idle.get();
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
        return m_xdgActivationIntegration.get();
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

    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<KWaylandServer::CompositorInterface> m_compositor;
    std::unique_ptr<KWaylandServer::SeatInterface> m_seat;
    std::unique_ptr<KWaylandServer::TabletManagerV2Interface> m_tabletManagerV2;
    std::unique_ptr<KWaylandServer::DataDeviceManagerInterface> m_dataDeviceManager;
    std::unique_ptr<KWaylandServer::PlasmaShellInterface> m_plasmaShell;
    std::unique_ptr<InputPanelV1Integration> m_inputPanelV1Integration;
    std::unique_ptr<XdgShellIntegration> m_xdgShellIntegration;
    std::unique_ptr<LayerShellV1Integration> m_layerShellV1Integration;
    std::unique_ptr<KWaylandServer::PlasmaWindowActivationFeedbackInterface> m_plasmaActivationFeedback;
    std::unique_ptr<KWaylandServer::PlasmaWindowManagementInterface> m_windowManagement;
    std::unique_ptr<KWaylandServer::PlasmaVirtualDesktopManagementInterface> m_virtualDesktopManagement;
    std::unique_ptr<KWaylandServer::ServerSideDecorationManagerInterface> m_decorationManager;
    std::unique_ptr<KWaylandServer::OutputManagementV2Interface> m_outputManagement;
    std::unique_ptr<KWaylandServer::AppMenuManagerInterface> m_appMenuManager;
    std::unique_ptr<KWaylandServer::ServerSideDecorationPaletteManagerInterface> m_paletteManager;
    std::unique_ptr<KWaylandServer::IdleInterface> m_idle;
    std::unique_ptr<KWaylandServer::XdgOutputManagerV1Interface> m_xdgOutputManagerV1;
    std::unique_ptr<KWaylandServer::XdgDecorationManagerV1Interface> m_xdgDecorationManagerV1;
    std::unique_ptr<KWaylandServer::LinuxDmaBufV1ClientBufferIntegration> m_linuxDmabuf;
    std::unique_ptr<KWaylandServer::KeyboardShortcutsInhibitManagerV1Interface> m_keyboardShortcutsInhibitManager;
    std::unique_ptr<KWaylandServer::InputMethodV1Interface> m_inputMethod;
    std::unique_ptr<KWaylandServer::XdgForeignV2Interface> m_XdgForeign;
    std::unique_ptr<KWaylandServer::PrimaryOutputV1Interface> m_primary;
    std::unique_ptr<XdgActivationV1Integration> m_xdgActivationIntegration;
    std::unique_ptr<KWaylandServer::XWaylandKeyboardGrabManagerV1Interface> m_xWaylandKeyboardGrabManager;
    std::unique_ptr<KWaylandServer::ViewporterInterface> m_viewPorter;
    std::unique_ptr<KWaylandServer::PointerGesturesV1Interface> m_pointerGestures;
    std::unique_ptr<KWaylandServer::PointerConstraintsV1Interface> m_pointerConstraints;
    std::unique_ptr<KWaylandServer::RelativePointerManagerV1Interface> m_relativePointerManager;
    std::unique_ptr<KWaylandServer::DataControlDeviceManagerV1Interface> m_dataControlDeviceManager;
    std::unique_ptr<KWaylandServer::PrimarySelectionDeviceManagerV1Interface> m_primarySelectionDeviceManager;
    std::unique_ptr<IdleInhibition> m_idleInhibition;
    std::unique_ptr<KWaylandServer::IdleInhibitManagerV1Interface> m_idleInhibtManager;
    std::unique_ptr<KWaylandServer::ShadowManagerInterface> m_shadowManager;
    std::unique_ptr<KWaylandServer::DpmsManagerInterface> m_dpmsManager;
    std::unique_ptr<KWaylandServer::SubCompositorInterface> m_subCompositor;
    std::unique_ptr<KWaylandServer::XdgActivationV1Interface> m_activationInterface;
    std::unique_ptr<KWaylandServer::KeyStateInterface> m_keyStateInterface;
    std::unique_ptr<KWaylandServer::LockscreenOverlayV1Interface> m_lockscreenOverlayInterface;
    std::map<Output *, std::unique_ptr<WaylandOutput>> m_waylandOutputs;
    QHash<Output *, KWaylandServer::OutputDeviceV2Interface *> m_waylandOutputDevices;
    QSet<KWaylandServer::LinuxDmaBufV1ClientBuffer *> m_linuxDmabufBuffers;
    QPointer<KWaylandServer::ClientConnection> m_xwaylandConnection;
    QPointer<KWaylandServer::ClientConnection> m_inputMethodServerConnection;
    KWaylandServer::ClientConnection *m_screenLockerClientConnection = nullptr;
    QList<Window *> m_windows;
    InitializationFlags m_initFlags;
    KWIN_SINGLETON(WaylandServer)
};

inline WaylandServer *waylandServer()
{
    return WaylandServer::self();
}

} // namespace KWin

#endif
