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
class AbstractOutput;

class XdgSurfaceConfigure
{
public:
    virtual ~XdgSurfaceConfigure() {}

    enum ConfigureFlag {
        ConfigurePosition = 0x1,
    };
    Q_DECLARE_FLAGS(ConfigureFlags, ConfigureFlag)

    QPoint position;
    qreal serial;
    ConfigureFlags flags;
};

class XdgSurfaceClient : public WaylandClient
{
    Q_OBJECT

public:
    explicit XdgSurfaceClient(KWaylandServer::XdgSurfaceInterface *shellSurface);
    ~XdgSurfaceClient() override;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    QRect frameRectToBufferRect(const QRect &rect) const override;
    QRect inputGeometry() const override;
    QMatrix4x4 inputTransformation() const override;
    void destroyClient() override;

    void installPlasmaShellSurface(KWaylandServer::PlasmaShellSurfaceInterface *shellSurface);

protected:
    void moveResizeInternal(const QRect &rect, MoveResizeMode mode) override;

    virtual XdgSurfaceConfigure *sendRoleConfigure() const = 0;
    virtual void handleRoleCommit();
    virtual void handleRolePrecommit();

    XdgSurfaceConfigure *lastAcknowledgedConfigure() const;
    void scheduleConfigure();
    void sendConfigure();

    QPointer<KWaylandServer::PlasmaShellSurfaceInterface> m_plasmaShellSurface;

    NET::WindowType m_windowType = NET::Normal;

private:
    void setupPlasmaShellIntegration();
    void updateClientArea();
    void updateShowOnScreenEdge();
    void handleConfigureAcknowledged(quint32 serial);
    void handleCommit();
    void handleNextWindowGeometry();
    bool haveNextWindowGeometry() const;
    void setHaveNextWindowGeometry();
    void resetHaveNextWindowGeometry();
    QRect adjustMoveResizeGeometry(const QRect &rect) const;
    void updateGeometryRestoreHack();
    void maybeUpdateMoveResizeGeometry(const QRect &rect);

    KWaylandServer::XdgSurfaceInterface *m_shellSurface;
    QTimer *m_configureTimer;
    XdgSurfaceConfigure::ConfigureFlags m_configureFlags;
    QQueue<XdgSurfaceConfigure *> m_configureEvents;
    QScopedPointer<XdgSurfaceConfigure> m_lastAcknowledgedConfigure;
    QRect m_windowGeometry;
    bool m_haveNextWindowGeometry = false;
};

class XdgToplevelConfigure final : public XdgSurfaceConfigure
{
public:
    QSharedPointer<KDecoration2::Decoration> decoration;
    KWaylandServer::XdgToplevelInterface::States states;
};

class XdgToplevelClient final : public XdgSurfaceClient
{
    Q_OBJECT

    enum class PingReason {
        CloseWindow,
        FocusWindow,
    };

    enum class DecorationMode {
        None,
        Client,
        Server,
    };

public:
    explicit XdgToplevelClient(KWaylandServer::XdgToplevelInterface *shellSurface);
    ~XdgToplevelClient() override;

    KWaylandServer::XdgToplevelInterface *shellSurface() const;

    MaximizeMode maximizeMode() const override;
    MaximizeMode requestedMaximizeMode() const override;
    QSize minSize() const override;
    QSize maxSize() const override;
    bool isFullScreen() const override;
    bool isRequestedFullScreen() const override;
    bool isMovableAcrossScreens() const override;
    bool isMovable() const override;
    bool isResizable() const override;
    bool isCloseable() const override;
    bool isFullScreenable() const override;
    bool isMaximizable() const override;
    bool isMinimizable() const override;
    bool isPlaceable() const override;
    bool isTransient() const override;
    bool userCanSetFullScreen() const override;
    bool userCanSetNoBorder() const override;
    bool noBorder() const override;
    void setNoBorder(bool set) override;
    void invalidateDecoration() override;
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
    void installXdgDecoration(KWaylandServer::XdgToplevelDecorationV1Interface *decoration);

protected:
    XdgSurfaceConfigure *sendRoleConfigure() const override;
    void handleRoleCommit() override;
    void handleRolePrecommit() override;
    void doMinimize() override;
    void doInteractiveResizeSync() override;
    void doSetActive() override;
    void doSetFullScreen();
    void doSetMaximized();
    bool doStartInteractiveMoveResize() override;
    void doFinishInteractiveMoveResize() override;
    bool acceptsFocus() const override;
    void changeMaximize(bool horizontal, bool vertical, bool adjust) override;
    Layer layerForDock() const override;
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
    void handleMaximumSizeChanged();
    void handleMinimumSizeChanged();
    void initialize();
    void updateMaximizeMode(MaximizeMode maximizeMode);
    void updateFullScreenMode(bool set);
    void sendPing(PingReason reason);
    MaximizeMode initialMaximizeMode() const;
    bool initialFullScreenMode() const;
    DecorationMode preferredDecorationMode() const;
    void configureDecoration();
    void configureXdgDecoration(DecorationMode decorationMode);
    void configureServerDecoration(DecorationMode decorationMode);
    void clearDecoration();

    QPointer<KWaylandServer::AppMenuInterface> m_appMenuInterface;
    QPointer<KWaylandServer::ServerSideDecorationPaletteInterface> m_paletteInterface;
    QPointer<KWaylandServer::ServerSideDecorationInterface> m_serverDecoration;
    QPointer<KWaylandServer::XdgToplevelDecorationV1Interface> m_xdgDecoration;
    KWaylandServer::XdgToplevelInterface *m_shellSurface;
    KWaylandServer::XdgToplevelInterface::States m_requestedStates;
    KWaylandServer::XdgToplevelInterface::States m_acknowledgedStates;
    KWaylandServer::XdgToplevelInterface::States m_initialStates;
    QMap<quint32, PingReason> m_pings;
    MaximizeMode m_maximizeMode = MaximizeRestore;
    MaximizeMode m_requestedMaximizeMode = MaximizeRestore;
    bool m_isFullScreen = false;
    bool m_isRequestedFullScreen = false;
    bool m_isInitialized = false;
    bool m_userNoBorder = false;
    bool m_isTransient = false;
    QPointer<AbstractOutput> m_fullScreenRequestedOutput;
    QSharedPointer<KDecoration2::Decoration> m_nextDecoration;
};

class XdgPopupClient final : public XdgSurfaceClient
{
    Q_OBJECT

public:
    explicit XdgPopupClient(KWaylandServer::XdgPopupInterface *shellSurface);
    ~XdgPopupClient() override;

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

protected:
    bool acceptsFocus() const override;
    XdgSurfaceConfigure *sendRoleConfigure() const override;

private:
    void handleGrabRequested(KWaylandServer::SeatInterface *seat, quint32 serial);
    void handleRepositionRequested(quint32 token);
    void initialize();
    void relayout();
    void updateReactive();

    KWaylandServer::XdgPopupInterface *m_shellSurface;
    bool m_haveExplicitGrab = false;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::XdgSurfaceConfigure::ConfigureFlags)
