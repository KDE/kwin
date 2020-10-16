/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandclient.h"

#include <KWaylandServer/xdgshell_interface.h>

#include <QQueue>
#include <QTimer>

namespace KWaylandServer
{
class AppMenuInterface;
class PlasmaShellSurfaceInterface;
class ServerSideDecorationInterface;
class ServerSideDecorationPaletteInterface;
class XdgToplevelDecorationV1Interface;
}

namespace KWin
{

class XdgSurfaceConfigure
{
public:
    enum ConfigureField {
        PositionField = 0x1,
        SizeField = 0x2,
    };
    Q_DECLARE_FLAGS(ConfigureFields, ConfigureField)

    ConfigureFields presentFields;
    QPoint position;
    QSize size;
    qreal serial;
};

class XdgSurfaceClient : public WaylandClient
{
    Q_OBJECT

public:
    explicit XdgSurfaceClient(KWaylandServer::XdgSurfaceInterface *shellSurface);
    ~XdgSurfaceClient() override;

    QRect frameRectToBufferRect(const QRect &rect) const override;
    QRect inputGeometry() const override;
    QMatrix4x4 inputTransformation() const override;
    bool isInitialPositionSet() const override;
    void destroyClient() override;
    void setVirtualKeyboardGeometry(const QRect &geo) override;

    virtual void installPlasmaShellSurface(KWaylandServer::PlasmaShellSurfaceInterface *shellSurface) = 0;

protected:
    void requestGeometry(const QRect &rect) override;
    void addDamage(const QRegion &damage) override;

    virtual XdgSurfaceConfigure *sendRoleConfigure() const = 0;
    virtual void handleRoleCommit();
    virtual bool stateCompare() const;

    XdgSurfaceConfigure *lastAcknowledgedConfigure() const;
    void scheduleConfigure();
    void sendConfigure();

    QPointer<KWaylandServer::PlasmaShellSurfaceInterface> m_plasmaShellSurface;

private:
    void handleConfigureAcknowledged(quint32 serial);
    void handleCommit();
    void handleNextWindowGeometry();
    bool haveNextWindowGeometry() const;
    void setHaveNextWindowGeometry();
    void resetHaveNextWindowGeometry();
    QRect adjustMoveResizeGeometry(const QRect &rect) const;
    void updateGeometryRestoreHack();

    KWaylandServer::XdgSurfaceInterface *m_shellSurface;
    QTimer *m_configureTimer;
    QQueue<XdgSurfaceConfigure *> m_configureEvents;
    QScopedPointer<XdgSurfaceConfigure> m_lastAcknowledgedConfigure;
    QRect m_windowGeometry;
    bool m_haveNextWindowGeometry = false;
};

class XdgToplevelConfigure final : public XdgSurfaceConfigure
{
public:
    KWaylandServer::XdgToplevelInterface::States states;
};

class XdgToplevelClient final : public XdgSurfaceClient
{
    Q_OBJECT

    enum class PingReason { CloseWindow, FocusWindow };

public:
    explicit XdgToplevelClient(KWaylandServer::XdgToplevelInterface *shellSurface);
    ~XdgToplevelClient() override;

    KWaylandServer::XdgToplevelInterface *shellSurface() const;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    MaximizeMode maximizeMode() const override;
    MaximizeMode requestedMaximizeMode() const override;
    QSize minSize() const override;
    QSize maxSize() const override;
    bool isFullScreen() const override;
    bool isMovableAcrossScreens() const override;
    bool isMovable() const override;
    bool isResizable() const override;
    bool isCloseable() const override;
    bool isFullScreenable() const override;
    bool isMaximizable() const override;
    bool isMinimizable() const override;
    bool isTransient() const override;
    bool userCanSetFullScreen() const override;
    bool userCanSetNoBorder() const override;
    bool noBorder() const override;
    void setNoBorder(bool set) override;
    void updateDecoration(bool check_workspace_pos, bool force = false) override;
    QString preferredColorScheme() const override;
    bool supportsWindowRules() const override;
    bool takeFocus() override;
    bool wantsInput() const override;
    bool dockWantsInput() const override;
    StrutRect strutRect(StrutArea area) const override;
    bool hasStrut() const override;
    void showOnScreenEdge() override;
    void setFullScreen(bool set, bool user) override;
    void closeWindow() override;

    void installAppMenu(KWaylandServer::AppMenuInterface *appMenu);
    void installServerDecoration(KWaylandServer::ServerSideDecorationInterface *decoration);
    void installPalette(KWaylandServer::ServerSideDecorationPaletteInterface *palette);
    void installPlasmaShellSurface(KWaylandServer::PlasmaShellSurfaceInterface *shellSurface) override;
    void installXdgDecoration(KWaylandServer::XdgToplevelDecorationV1Interface *decoration);

protected:
    XdgSurfaceConfigure *sendRoleConfigure() const override;
    void handleRoleCommit() override;
    void doMinimize() override;
    void doResizeSync() override;
    void doSetActive() override;
    void doSetFullScreen();
    void doSetMaximized();
    bool doStartMoveResize() override;
    void doFinishMoveResize() override;
    bool acceptsFocus() const override;
    void changeMaximize(bool horizontal, bool vertical, bool adjust) override;
    Layer layerForDock() const override;
    bool stateCompare() const override;
    void doSetQuickTileMode() override;

private:
    void handleWindowTitleChanged();
    void handleWindowClassChanged();
    void handleWindowMenuRequested(KWaylandServer::SeatInterface *seat,
                                   const QPoint &surfacePos, quint32 serial);
    void handleMoveRequested(KWaylandServer::SeatInterface *seat, quint32 serial);
    void handleResizeRequested(KWaylandServer::SeatInterface *seat, Qt::Edges, quint32 serial);
    void handleStatesAcknowledged(const KWaylandServer::XdgToplevelInterface::States &states);
    void handleMaximizeRequested();
    void handleUnmaximizeRequested();
    void handleFullscreenRequested(KWaylandServer::OutputInterface *output);
    void handleUnfullscreenRequested();
    void handleMinimizeRequested();
    void handleTransientForChanged();
    void handleForeignTransientForChanged(KWaylandServer::SurfaceInterface *child);
    void handlePingTimeout(quint32 serial);
    void handlePingDelayed(quint32 serial);
    void handlePongReceived(quint32 serial);
    void initialize();
    void updateMaximizeMode(MaximizeMode maximizeMode);
    void updateFullScreenMode(bool set);
    void updateShowOnScreenEdge();
    void updateClientArea();
    void setupWindowManagementIntegration();
    void setupPlasmaShellIntegration();
    void sendPing(PingReason reason);
    MaximizeMode initialMaximizeMode() const;
    bool initialFullScreenMode() const;

    QPointer<KWaylandServer::AppMenuInterface> m_appMenuInterface;
    QPointer<KWaylandServer::ServerSideDecorationPaletteInterface> m_paletteInterface;
    QPointer<KWaylandServer::ServerSideDecorationInterface> m_serverDecoration;
    QPointer<KWaylandServer::XdgToplevelDecorationV1Interface> m_xdgDecoration;
    KWaylandServer::XdgToplevelInterface *m_shellSurface;
    KWaylandServer::XdgToplevelInterface::States m_requestedStates;
    KWaylandServer::XdgToplevelInterface::States m_acknowledgedStates;
    KWaylandServer::XdgToplevelInterface::States m_initialStates;
    QMap<quint32, PingReason> m_pings;
    QRect m_fullScreenGeometryRestore;
    NET::WindowType m_windowType = NET::Normal;
    MaximizeMode m_maximizeMode = MaximizeRestore;
    MaximizeMode m_requestedMaximizeMode = MaximizeRestore;
    bool m_isFullScreen = false;
    bool m_isInitialized = false;
    bool m_userNoBorder = false;
    bool m_isTransient = false;
};

class XdgPopupClient final : public XdgSurfaceClient
{
    Q_OBJECT

public:
    explicit XdgPopupClient(KWaylandServer::XdgPopupInterface *shellSurface);
    ~XdgPopupClient() override;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    bool hasPopupGrab() const override;
    void popupDone() override;
    bool isPopupWindow() const override;
    bool isTransient() const override;
    bool isResizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool hasTransientPlacementHint() const override;
    QRect transientPlacement(const QRect &bounds) const override;
    bool isCloseable() const override;
    void closeWindow() override;
    bool wantsInput() const override;
    bool takeFocus() override;
    void installPlasmaShellSurface(KWaylandServer::PlasmaShellSurfaceInterface *shellSurface) override;

protected:
    bool acceptsFocus() const override;
    XdgSurfaceConfigure *sendRoleConfigure() const override;

private:
    void handleGrabRequested(KWaylandServer::SeatInterface *seat, quint32 serial);
    void initialize();

    KWaylandServer::XdgPopupInterface *m_shellSurface;
    bool m_haveExplicitGrab = false;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::XdgSurfaceConfigure::ConfigureFields)
