/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/xdgshell.h"
#include "waylandwindow.h"

#include <QQueue>
#include <QTimer>

#include <optional>

namespace KDecoration3
{
class DecorationState;
}

namespace KWin
{

class AppMenuInterface;
class KillPrompt;
class PlasmaShellSurfaceInterface;
class ServerSideDecorationInterface;
class ServerSideDecorationPaletteInterface;
class XdgDialogV1Interface;
class XdgToplevelDecorationV1Interface;
class Output;
class Tile;

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
    double scale;
};

class XdgSurfaceWindow : public WaylandWindow
{
    Q_OBJECT

public:
    explicit XdgSurfaceWindow(XdgSurfaceInterface *shellSurface);
    ~XdgSurfaceWindow() override;

    WindowType windowType() const override;
    QRectF frameRectToBufferRect(const QRectF &rect) const override;
    void destroyWindow() override;

    void installPlasmaShellSurface(PlasmaShellSurfaceInterface *shellSurface);

protected:
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;

    virtual XdgSurfaceConfigure *sendRoleConfigure() const = 0;
    virtual void handleRoleCommit();
    virtual void handleRolePrecommit();
    virtual void handleRoleDestroyed();

    XdgSurfaceConfigure *lastAcknowledgedConfigure() const;
    void scheduleConfigure();
    void sendConfigure();

    QPointer<PlasmaShellSurfaceInterface> m_plasmaShellSurface;

    WindowType m_windowType = WindowType::Normal;
    Gravity m_nextGravity = Gravity::None;

private:
    void handleConfigureAcknowledged(quint32 serial);
    void handleCommit();
    void handleNextWindowGeometry();
    bool haveNextWindowGeometry() const;
    void setHaveNextWindowGeometry();
    void resetHaveNextWindowGeometry();
    void maybeUpdateMoveResizeGeometry(const QRectF &rect);

    XdgSurfaceInterface *m_shellSurface;
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
    std::shared_ptr<KDecoration3::Decoration> decoration;
    std::shared_ptr<KDecoration3::DecorationState> decorationState;
    XdgToplevelInterface::States states;
    QPointer<Tile> tile = nullptr;
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
    explicit XdgToplevelWindow(XdgToplevelInterface *shellSurface);
    ~XdgToplevelWindow() override;

    XdgToplevelInterface *shellSurface() const;

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
    bool userCanSetNoBorder() const override;
    bool noBorder() const override;
    void setNoBorder(bool set) override;
    KDecoration3::Decoration *nextDecoration() const override;
    void invalidateDecoration() override;
    QString preferredColorScheme() const override;
    bool supportsWindowRules() const override;
    void applyWindowRules() override;
    bool takeFocus() override;
    bool wantsInput() const override;
    bool dockWantsInput() const override;
    void setFullScreen(bool set) override;
    void closeWindow() override;
    void maximize(MaximizeMode mode, const QRectF &restore = QRectF()) override;

    void installAppMenu(AppMenuInterface *appMenu);
    void installServerDecoration(ServerSideDecorationInterface *decoration);
    void installPalette(ServerSideDecorationPaletteInterface *palette);
    void installXdgDecoration(XdgToplevelDecorationV1Interface *decoration);
    void installXdgDialogV1(XdgDialogV1Interface *dialog);

protected:
    XdgSurfaceConfigure *sendRoleConfigure() const override;
    void handleRoleCommit() override;
    void handleRolePrecommit() override;
    void handleRoleDestroyed() override;
    void doMinimize() override;
    void doInteractiveResizeSync(const QRectF &rect) override;
    void doSetActive() override;
    void doSetFullScreen();
    void doSetMaximized();
    bool doStartInteractiveMoveResize() override;
    void doFinishInteractiveMoveResize() override;
    bool acceptsFocus() const override;
    void doSetQuickTileMode() override;
    void doSetSuspended() override;
    void doSetNextTargetScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;

private:
    void handleWindowTitleChanged();
    void handleWindowClassChanged();
    void handleWindowMenuRequested(SeatInterface *seat,
                                   const QPoint &surfacePos, quint32 serial);
    void handleMoveRequested(SeatInterface *seat, quint32 serial);
    void handleResizeRequested(SeatInterface *seat, XdgToplevelInterface::ResizeAnchor anchor, quint32 serial);
    void handleStatesAcknowledged(const XdgToplevelInterface::States &states);
    void handleMaximizeRequested();
    void handleUnmaximizeRequested();
    void handleFullscreenRequested(OutputInterface *output);
    void handleUnfullscreenRequested();
    void handleMinimizeRequested();
    void handleTransientForChanged();
    void handleForeignTransientForChanged(SurfaceInterface *child);
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
    void processDecorationState(std::shared_ptr<KDecoration3::DecorationState> state);
    void updateCapabilities();
    void updateIcon();

    QPointer<AppMenuInterface> m_appMenuInterface;
    QPointer<ServerSideDecorationPaletteInterface> m_paletteInterface;
    QPointer<ServerSideDecorationInterface> m_serverDecoration;
    QPointer<XdgToplevelDecorationV1Interface> m_xdgDecoration;
    QPointer<XdgDialogV1Interface> m_xdgDialog;
    XdgToplevelInterface *m_shellSurface;
    XdgToplevelInterface::States m_nextStates;
    XdgToplevelInterface::States m_acknowledgedStates;
    XdgToplevelInterface::States m_initialStates;
    XdgToplevelInterface::Capabilities m_capabilities;
    QMap<quint32, PingReason> m_pings;
    MaximizeMode m_maximizeMode = MaximizeRestore;
    MaximizeMode m_requestedMaximizeMode = MaximizeRestore;
    QSizeF m_minimumSize = QSizeF(0, 0);
    QSizeF m_maximumSize = QSizeF(0, 0);
    bool m_isFullScreen = false;
    bool m_isRequestedFullScreen = false;
    bool m_isInitialized = false;
    bool m_userNoBorder = false;
    bool m_isTransient = false;
    QPointer<Output> m_fullScreenRequestedOutput;
    std::shared_ptr<KDecoration3::Decoration> m_nextDecoration;
    std::shared_ptr<KDecoration3::DecorationState> m_nextDecorationState;
    std::unique_ptr<KillPrompt> m_killPrompt;
};

class XdgPopupWindow final : public XdgSurfaceWindow
{
    Q_OBJECT

public:
    explicit XdgPopupWindow(XdgPopupInterface *shellSurface);
    ~XdgPopupWindow() override;

    bool hasPopupGrab() const override;
    void popupDone() override;
    bool isPopupWindow() const override;
    bool isTransient() const override;
    bool isResizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool hasTransientPlacementHint() const override;
    QRectF transientPlacement() const override;
    bool isCloseable() const override;
    void closeWindow() override;
    bool wantsInput() const override;
    bool takeFocus() override;

protected:
    bool acceptsFocus() const override;
    XdgSurfaceConfigure *sendRoleConfigure() const override;
    void handleRoleDestroyed() override;
    void doSetNextTargetScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;

private:
    void handleGrabRequested(SeatInterface *seat, quint32 serial);
    void handleRepositionRequested(quint32 token);
    void initialize();
    void updateRelativePlacement();
    void relayout();

    XdgPopupInterface *m_shellSurface;
    bool m_haveExplicitGrab = false;
    QRectF m_relativePlacement;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::XdgSurfaceConfigure::ConfigureFlags)
