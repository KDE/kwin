/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/xdgshell_interface.h"
#include "waylandwindow.h"

#include <QQueue>
#include <QTimer>

#include <optional>

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
class Output;

class XdgSurfaceConfigure
{
public:
    virtual ~XdgSurfaceConfigure()
    {
    }

    enum ConfigureFlag {
        ConfigurePosition = 0x1,
    };
    Q_DECLARE_FLAGS(ConfigureFlags, ConfigureFlag)

    QRectF bounds;
    Gravity gravity;
    qreal serial;
    ConfigureFlags flags;
};

class XdgSurfaceWindow : public WaylandWindow
{
    Q_OBJECT

public:
    explicit XdgSurfaceWindow(KWaylandServer::XdgSurfaceInterface *shellSurface);
    ~XdgSurfaceWindow() override;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    QRectF frameRectToBufferRect(const QRectF &rect) const override;
    QRectF inputGeometry() const override;
    QMatrix4x4 inputTransformation() const override;
    void destroyWindow() override;

    void installPlasmaShellSurface(KWaylandServer::PlasmaShellSurfaceInterface *shellSurface);

protected:
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;

    virtual XdgSurfaceConfigure *sendRoleConfigure() const = 0;
    virtual void handleRoleCommit();
    virtual void handleRolePrecommit();

    XdgSurfaceConfigure *lastAcknowledgedConfigure() const;
    void scheduleConfigure();
    void sendConfigure();

    QPointer<KWaylandServer::PlasmaShellSurfaceInterface> m_plasmaShellSurface;

    NET::WindowType m_windowType = NET::Normal;
    Gravity m_nextGravity = Gravity::None;

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
    void maybeUpdateMoveResizeGeometry(const QRectF &rect);

    KWaylandServer::XdgSurfaceInterface *m_shellSurface;
    QTimer *m_configureTimer;
    XdgSurfaceConfigure::ConfigureFlags m_configureFlags;
    QQueue<XdgSurfaceConfigure *> m_configureEvents;
    std::unique_ptr<XdgSurfaceConfigure> m_lastAcknowledgedConfigure;
    std::optional<quint32> m_lastAcknowledgedConfigureSerial;
    QRectF m_windowGeometry;
    bool m_haveNextWindowGeometry = false;
};

class XdgToplevelConfigure final : public XdgSurfaceConfigure
{
public:
    std::shared_ptr<KDecoration2::Decoration> decoration;
    KWaylandServer::XdgToplevelInterface::States states;
};

class XdgToplevelWindow final : public XdgSurfaceWindow
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
    explicit XdgToplevelWindow(KWaylandServer::XdgToplevelInterface *shellSurface);
    ~XdgToplevelWindow() override;

    KWaylandServer::XdgToplevelInterface *shellSurface() const;

    MaximizeMode maximizeMode() const override;
    MaximizeMode requestedMaximizeMode() const override;
    QSizeF minSize() const override;
    QSizeF maxSize() const override;
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
    void maximize(MaximizeMode mode) override;

    void installAppMenu(KWaylandServer::AppMenuInterface *appMenu);
    void installServerDecoration(KWaylandServer::ServerSideDecorationInterface *decoration);
    void installPalette(KWaylandServer::ServerSideDecorationPaletteInterface *palette);
    void installXdgDecoration(KWaylandServer::XdgToplevelDecorationV1Interface *decoration);

protected:
    XdgSurfaceConfigure *sendRoleConfigure() const override;
    void handleRoleCommit() override;
    void handleRolePrecommit() override;
    void doMinimize() override;
    void doInteractiveResizeSync(const QRectF &rect) override;
    void doSetActive() override;
    void doSetFullScreen();
    void doSetMaximized();
    bool doStartInteractiveMoveResize() override;
    void doFinishInteractiveMoveResize() override;
    bool acceptsFocus() const override;
    Layer layerForDock() const override;
    void doSetQuickTileMode() override;

private:
    void handleWindowTitleChanged();
    void handleWindowClassChanged();
    void handleWindowMenuRequested(KWaylandServer::SeatInterface *seat,
                                   const QPoint &surfacePos, quint32 serial);
    void handleMoveRequested(KWaylandServer::SeatInterface *seat, quint32 serial);
    void handleResizeRequested(KWaylandServer::SeatInterface *seat, KWaylandServer::XdgToplevelInterface::ResizeAnchor anchor, quint32 serial);
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
    KWaylandServer::XdgToplevelInterface::States m_nextStates;
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
    QPointer<Output> m_fullScreenRequestedOutput;
    std::shared_ptr<KDecoration2::Decoration> m_nextDecoration;
};

class XdgPopupWindow final : public XdgSurfaceWindow
{
    Q_OBJECT

public:
    explicit XdgPopupWindow(KWaylandServer::XdgPopupInterface *shellSurface);
    ~XdgPopupWindow() override;

    bool hasPopupGrab() const override;
    void popupDone() override;
    bool isPopupWindow() const override;
    bool isTransient() const override;
    bool isResizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool hasTransientPlacementHint() const override;
    QRectF transientPlacement(const QRectF &bounds) const override;
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
