/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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

    QRect inputGeometry() const override;
    QRect bufferGeometry() const override;
    QMatrix4x4 inputTransformation() const override;
    void setFrameGeometry(const QRect &rect, ForceGeometry_t force = NormalGeometrySet) override;
    using AbstractClient::move;
    void move(int x, int y, ForceGeometry_t force = NormalGeometrySet) override;
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override;
    void hideClient(bool hide) override;
    void destroyClient() override;

    QRect frameRectToBufferRect(const QRect &rect) const;
    QRect requestedFrameGeometry() const;
    QPoint requestedPos() const;
    QSize requestedSize() const;
    QRect requestedClientGeometry() const;
    QSize requestedClientSize() const;
    QRect clientGeometry() const;
    bool isHidden() const;

protected:
    void addDamage(const QRegion &damage) override;

    virtual XdgSurfaceConfigure *sendRoleConfigure() const = 0;
    virtual void handleRoleCommit();
    virtual bool stateCompare() const;

    XdgSurfaceConfigure *lastAcknowledgedConfigure() const;
    void scheduleConfigure();
    void sendConfigure();
    void requestGeometry(const QRect &rect);
    void updateGeometry(const QRect &rect);

private:
    void handleConfigureAcknowledged(quint32 serial);
    void handleCommit();
    void handleNextWindowGeometry();
    bool haveNextWindowGeometry() const;
    void setHaveNextWindowGeometry();
    void resetHaveNextWindowGeometry();
    QRect adjustMoveResizeGeometry(const QRect &rect) const;
    void updateGeometryRestoreHack();
    void updateDepth();
    void internalShow();
    void internalHide();
    void cleanGrouping();
    void cleanTabBox();

    KWaylandServer::XdgSurfaceInterface *m_shellSurface;
    QTimer *m_configureTimer;
    QQueue<XdgSurfaceConfigure *> m_configureEvents;
    QScopedPointer<XdgSurfaceConfigure> m_lastAcknowledgedConfigure;
    QRect m_windowGeometry;
    QRect m_requestedFrameGeometry;
    QRect m_bufferGeometry;
    QRect m_requestedClientGeometry;
    bool m_isHidden = false;
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

    void debug(QDebug &stream) const override;
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
    void updateColorScheme() override;
    bool supportsWindowRules() const override;
    bool takeFocus() override;
    bool wantsInput() const override;
    bool dockWantsInput() const override;
    bool hasStrut() const override;
    void showOnScreenEdge() override;
    bool isInitialPositionSet() const override;
    void setFullScreen(bool set, bool user) override;
    void closeWindow() override;

    void installAppMenu(KWaylandServer::AppMenuInterface *appMenu);
    void installServerDecoration(KWaylandServer::ServerSideDecorationInterface *decoration);
    void installPalette(KWaylandServer::ServerSideDecorationPaletteInterface *palette);
    void installPlasmaShellSurface(KWaylandServer::PlasmaShellSurfaceInterface *shellSurface);
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
    void setupWindowManagementIntegration();
    void setupPlasmaShellIntegration();
    void sendPing(PingReason reason);
    MaximizeMode initialMaximizeMode() const;
    bool initialFullScreenMode() const;

    QPointer<KWaylandServer::PlasmaShellSurfaceInterface> m_plasmaShellSurface;
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

    void debug(QDebug &stream) const override;
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
    void updateColorScheme() override;
    bool noBorder() const override;
    bool userCanSetNoBorder() const override;
    void setNoBorder(bool set) override;
    void updateDecoration(bool check_workspace_pos, bool force = false) override;
    void showOnScreenEdge() override;
    bool wantsInput() const override;
    bool takeFocus() override;
    bool supportsWindowRules() const override;

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
