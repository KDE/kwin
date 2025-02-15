/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xdgshellwindow.h"
#include "core/output.h"
#include "effect/globals.h"
#include "utils/common.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "core/pixelgrid.h"
#include "decorations/decorationbridge.h"
#include "killprompt.h"
#include "placement.h"
#include "pointer_input.h"
#include "tablet_input.h"
#include "tiles/tilemanager.h"
#include "touch_input.h"
#include "utils/subsurfacemonitor.h"
#include "virtualdesktops.h"
#include "wayland/appmenu.h"
#include "wayland/output.h"
#include "wayland/plasmashell.h"
#include "wayland/seat.h"
#include "wayland/server_decoration.h"
#include "wayland/server_decoration_palette.h"
#include "wayland/surface.h"
#include "wayland/tablet_v2.h"
#include "wayland/xdgdecoration_v1.h"
#include "wayland/xdgdialog_v1.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>

namespace KWin
{

XdgSurfaceWindow::XdgSurfaceWindow(XdgSurfaceInterface *shellSurface)
    : WaylandWindow(shellSurface->surface())
    , m_shellSurface(shellSurface)
    , m_configureTimer(new QTimer(this))
{
    connect(shellSurface, &XdgSurfaceInterface::configureAcknowledged,
            this, &XdgSurfaceWindow::handleConfigureAcknowledged);
    connect(shellSurface, &XdgSurfaceInterface::resetOccurred,
            this, &XdgSurfaceWindow::destroyWindow);
    connect(shellSurface->surface(), &SurfaceInterface::committed,
            this, &XdgSurfaceWindow::handleCommit);
    connect(shellSurface, &XdgSurfaceInterface::aboutToBeDestroyed,
            this, &XdgSurfaceWindow::destroyWindow);
    connect(shellSurface->surface(), &SurfaceInterface::aboutToBeDestroyed,
            this, &XdgSurfaceWindow::destroyWindow);

    // The effective window geometry is determined by two things: (a) the rectangle that bounds
    // the main surface and all of its sub-surfaces, (b) the client-specified window geometry, if
    // any. If the client hasn't provided the window geometry, we fallback to the bounding sub-
    // surface rectangle. If the client has provided the window geometry, we intersect it with
    // the bounding rectangle and that will be the effective window geometry. It's worth to point
    // out that geometry updates do not occur that frequently, so we don't need to recompute the
    // bounding geometry every time the client commits the surface.

    SubSurfaceMonitor *treeMonitor = new SubSurfaceMonitor(surface(), this);

    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceAdded,
            this, &XdgSurfaceWindow::setHaveNextWindowGeometry);
    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceRemoved,
            this, &XdgSurfaceWindow::setHaveNextWindowGeometry);
    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceMoved,
            this, &XdgSurfaceWindow::setHaveNextWindowGeometry);
    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceResized,
            this, &XdgSurfaceWindow::setHaveNextWindowGeometry);
    connect(shellSurface, &XdgSurfaceInterface::windowGeometryChanged,
            this, &XdgSurfaceWindow::setHaveNextWindowGeometry);
    connect(surface(), &SurfaceInterface::sizeChanged,
            this, &XdgSurfaceWindow::setHaveNextWindowGeometry);

    // Configure events are not sent immediately, but rather scheduled to be sent when the event
    // loop is about to be idle. By doing this, we can avoid sending configure events that do
    // nothing, and implementation-wise, it's simpler.

    m_configureTimer->setSingleShot(true);
    connect(m_configureTimer, &QTimer::timeout, this, &XdgSurfaceWindow::sendConfigure);
}

XdgSurfaceWindow::~XdgSurfaceWindow()
{
}

WindowType XdgSurfaceWindow::windowType() const
{
    return m_windowType;
}

XdgSurfaceConfigure *XdgSurfaceWindow::lastAcknowledgedConfigure() const
{
    return m_lastAcknowledgedConfigure.get();
}

void XdgSurfaceWindow::scheduleConfigure()
{
    if (!isDeleted()) {
        m_configureTimer->start();
    }
}

void XdgSurfaceWindow::sendConfigure()
{
    XdgSurfaceConfigure *configureEvent = sendRoleConfigure();

    // The configure event inherits configure flags from the previous event.
    if (!m_configureEvents.isEmpty()) {
        const XdgSurfaceConfigure *previousEvent = m_configureEvents.constLast();
        configureEvent->flags = previousEvent->flags;
    }

    configureEvent->gravity = m_nextGravity;
    configureEvent->flags |= m_configureFlags;
    configureEvent->scale = m_nextTargetScale;
    m_configureFlags = {};

    m_configureEvents.append(configureEvent);
}

void XdgSurfaceWindow::handleConfigureAcknowledged(quint32 serial)
{
    m_lastAcknowledgedConfigureSerial = serial;
}

void XdgSurfaceWindow::handleCommit()
{
    if (!surface()->buffer()) {
        return;
    }

    if (m_lastAcknowledgedConfigureSerial.has_value()) {
        const quint32 serial = m_lastAcknowledgedConfigureSerial.value();
        while (!m_configureEvents.isEmpty()) {
            if (serial < m_configureEvents.constFirst()->serial) {
                break;
            }
            m_lastAcknowledgedConfigure.reset(m_configureEvents.takeFirst());
        }
    }

    handleRolePrecommit();
    if (haveNextWindowGeometry()) {
        handleNextWindowGeometry();
        resetHaveNextWindowGeometry();
    }

    handleRoleCommit();
    m_lastAcknowledgedConfigure.reset();
    m_lastAcknowledgedConfigureSerial.reset();

    markAsMapped();
}

void XdgSurfaceWindow::handleRolePrecommit()
{
}

void XdgSurfaceWindow::handleRoleCommit()
{
}

void XdgSurfaceWindow::maybeUpdateMoveResizeGeometry(const QRectF &rect)
{
    // We are about to send a configure event, ignore the committed window geometry.
    if (m_configureTimer->isActive()) {
        return;
    }

    // If there are unacknowledged configure events that change the geometry, don't sync
    // the move resize geometry in order to avoid rolling back to old state. When the last
    // configure event is acknowledged, the move resize geometry will be synchronized.
    for (int i = m_configureEvents.count() - 1; i >= 0; --i) {
        if (m_configureEvents[i]->flags & XdgSurfaceConfigure::ConfigurePosition) {
            return;
        }
    }

    setMoveResizeGeometry(rect);
}

void XdgSurfaceWindow::handleNextWindowGeometry()
{
    if (const XdgSurfaceConfigure *configureEvent = lastAcknowledgedConfigure()) {
        setTargetScale(configureEvent->scale);
    }
    const QRectF boundingGeometry = surface()->boundingRect();

    // The effective window geometry is defined as the intersection of the window geometry
    // and the rectangle that bounds the main surface and all of its sub-surfaces. If the
    // client hasn't specified the window geometry, we must fallback to the bounding geometry.
    // Note that the xdg-shell spec is not clear about when exactly we have to clamp the
    // window geometry.

    m_windowGeometry = m_shellSurface->windowGeometry();
    if (m_windowGeometry.isValid()) {
        m_windowGeometry &= boundingGeometry;
    } else {
        m_windowGeometry = boundingGeometry;
    }

    if (m_windowGeometry.isEmpty()) {
        qCWarning(KWIN_CORE) << "Committed empty window geometry, dealing with a buggy client!";
    } else {
        m_windowGeometry = snapToPixels(m_windowGeometry, targetScale());
    }

    QRectF frameGeometry(pos(), clientSizeToFrameSize(m_windowGeometry.size()));
    if (const XdgSurfaceConfigure *configureEvent = lastAcknowledgedConfigure()) {
        if (configureEvent->flags & XdgSurfaceConfigure::ConfigurePosition) {
            frameGeometry = gravitateGeometry(frameGeometry, configureEvent->bounds, configureEvent->gravity);
        }
    }

    if (isInteractiveMove()) {
        bool fullscreen = isFullScreen();
        if (const auto configureEvent = static_cast<XdgToplevelConfigure *>(lastAcknowledgedConfigure())) {
            fullscreen = configureEvent->states & XdgToplevelInterface::State::FullScreen;
        }
        if (!fullscreen) {
            const QPointF anchor = interactiveMoveResizeAnchor();
            const QPointF offset = interactiveMoveOffset();
            frameGeometry.moveTopLeft(QPointF(anchor.x() - offset.x() * frameGeometry.width(),
                                              anchor.y() - offset.y() * frameGeometry.height()));
        }
    }

    maybeUpdateMoveResizeGeometry(frameGeometry);
    updateGeometry(frameGeometry);
}

bool XdgSurfaceWindow::haveNextWindowGeometry() const
{
    return m_haveNextWindowGeometry || m_lastAcknowledgedConfigure;
}

void XdgSurfaceWindow::setHaveNextWindowGeometry()
{
    m_haveNextWindowGeometry = true;
}

void XdgSurfaceWindow::resetHaveNextWindowGeometry()
{
    m_haveNextWindowGeometry = false;
}

void XdgSurfaceWindow::moveResizeInternal(const QRectF &rect, MoveResizeMode mode)
{
    Q_EMIT frameGeometryAboutToChange();

    if (mode != MoveResizeMode::Move) {
        // xdg-shell doesn't support fractional sizes, so the requested
        // and current client sizes have to be rounded to integers
        const QSizeF requestedFrameSize = snapToPixels(rect.size(), nextTargetScale());
        const QSizeF requestedClientSize = nextFrameSizeToClientSize(requestedFrameSize);
        if (requestedClientSize.toSize() == clientSize().toSize()) {
            updateGeometry(QRectF(rect.topLeft(), requestedFrameSize));
        } else {
            m_configureFlags |= XdgSurfaceConfigure::ConfigurePosition;
            scheduleConfigure();
        }
    } else {
        // If the window is moved, cancel any queued window position updates.
        for (XdgSurfaceConfigure *configureEvent : std::as_const(m_configureEvents)) {
            configureEvent->flags.setFlag(XdgSurfaceConfigure::ConfigurePosition, false);
        }
        m_configureFlags.setFlag(XdgSurfaceConfigure::ConfigurePosition, false);
        updateGeometry(QRectF(rect.topLeft(), size()));
    }
}

QRectF XdgSurfaceWindow::frameRectToBufferRect(const QRectF &rect) const
{
    const qreal left = rect.left() + borderLeft() - m_windowGeometry.left();
    const qreal top = rect.top() + borderTop() - m_windowGeometry.top();
    return QRectF(QPointF(left, top), snapToPixels(surface()->size(), m_targetScale));
}

void XdgSurfaceWindow::handleRoleDestroyed()
{
    if (m_plasmaShellSurface) {
        m_plasmaShellSurface->disconnect(this);
    }
    m_shellSurface->disconnect(this);
    m_shellSurface->surface()->disconnect(this);
}

void XdgSurfaceWindow::destroyWindow()
{
    handleRoleDestroyed();
    markAsDeleted();
    Q_EMIT closed();

    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize()) {
        leaveInteractiveMoveResize();
        Q_EMIT interactiveMoveResizeFinished();
    }
    commitTile(nullptr);
    m_configureTimer->stop();
    qDeleteAll(m_configureEvents);
    m_configureEvents.clear();
    cleanTabBox();
    StackingUpdatesBlocker blocker(workspace());
    workspace()->rulebook()->discardUsed(this, true);
    waylandServer()->removeWindow(this);
    cleanGrouping();

    unref();
}

/**
 * \todo This whole plasma shell surface thing doesn't seem right. It turns xdg-toplevel into
 * something completely different! Perhaps plasmashell surfaces need to be implemented via a
 * proprietary protocol that doesn't piggyback on existing shell surface protocols. It'll lead
 * to cleaner code and will be technically correct, but I'm not sure whether this is do-able.
 */
void XdgSurfaceWindow::installPlasmaShellSurface(PlasmaShellSurfaceInterface *shellSurface)
{
    m_plasmaShellSurface = shellSurface;

    auto updatePosition = [this, shellSurface] {
        if (!isInteractiveMoveResize()) {
            move(shellSurface->position());
        }
    };
    auto showUnderCursor = [this] {
        // Wait for the first commit
        auto moveUnderCursor = [this] {
            if (input()->hasPointer()) {
                move(input()->globalPointer());
                moveResize(keepInArea(moveResizeGeometry(), workspace()->clientArea(PlacementArea, this)));
            }
        };
        connect(this, &Window::readyForPaintingChanged, this, moveUnderCursor, Qt::SingleShotConnection);
    };
    auto updateRole = [this, shellSurface] {
        WindowType type = WindowType::Unknown;
        switch (shellSurface->role()) {
        case PlasmaShellSurfaceInterface::Role::Desktop:
            type = WindowType::Desktop;
            break;
        case PlasmaShellSurfaceInterface::Role::Panel:
            type = WindowType::Dock;
            break;
        case PlasmaShellSurfaceInterface::Role::OnScreenDisplay:
            type = WindowType::OnScreenDisplay;
            break;
        case PlasmaShellSurfaceInterface::Role::Notification:
            type = WindowType::Notification;
            break;
        case PlasmaShellSurfaceInterface::Role::ToolTip:
            type = WindowType::Tooltip;
            break;
        case PlasmaShellSurfaceInterface::Role::CriticalNotification:
            type = WindowType::CriticalNotification;
            break;
        case PlasmaShellSurfaceInterface::Role::AppletPopup:
            type = WindowType::AppletPopup;
            break;
        case PlasmaShellSurfaceInterface::Role::Normal:
        default:
            type = WindowType::Normal;
            break;
        }
        if (m_windowType == type) {
            return;
        }
        m_windowType = type;
        switch (m_windowType) {
        case WindowType::Desktop:
        case WindowType::Dock:
        case WindowType::OnScreenDisplay:
        case WindowType::Notification:
        case WindowType::CriticalNotification:
        case WindowType::Tooltip:
        case WindowType::AppletPopup:
            setOnAllDesktops(true);
#if KWIN_BUILD_ACTIVITIES
            setOnAllActivities(true);
#endif
            break;
        default:
            break;
        }
    };
    connect(shellSurface, &PlasmaShellSurfaceInterface::positionChanged, this, updatePosition);
    connect(shellSurface, &PlasmaShellSurfaceInterface::openUnderCursorRequested, this, showUnderCursor);
    connect(shellSurface, &PlasmaShellSurfaceInterface::roleChanged, this, updateRole);
    connect(shellSurface, &PlasmaShellSurfaceInterface::panelTakesFocusChanged, this, [this] {
        if (m_plasmaShellSurface->panelTakesFocus()) {
            workspace()->activateWindow(this);
        }
    });
    if (shellSurface->isPositionSet()) {
        updatePosition();
    }
    if (shellSurface->wantsOpenUnderCursor()) {
        showUnderCursor();
    }
    updateRole();

    setSkipTaskbar(shellSurface->skipTaskbar());
    connect(shellSurface, &PlasmaShellSurfaceInterface::skipTaskbarChanged, this, [this] {
        setSkipTaskbar(m_plasmaShellSurface->skipTaskbar());
    });

    setSkipSwitcher(shellSurface->skipSwitcher());
    connect(shellSurface, &PlasmaShellSurfaceInterface::skipSwitcherChanged, this, [this] {
        setSkipSwitcher(m_plasmaShellSurface->skipSwitcher());
    });
}

XdgToplevelWindow::XdgToplevelWindow(XdgToplevelInterface *shellSurface)
    : XdgSurfaceWindow(shellSurface->xdgSurface())
    , m_shellSurface(shellSurface)
{
    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());
    setDesktops({VirtualDesktopManager::self()->currentDesktop()});
#if KWIN_BUILD_ACTIVITIES
    if (auto a = Workspace::self()->activities()) {
        setOnActivities({a->current()});
    }
#endif
    move(workspace()->activeOutput()->geometry().center());

    connect(shellSurface, &XdgToplevelInterface::windowTitleChanged,
            this, &XdgToplevelWindow::handleWindowTitleChanged);
    connect(shellSurface, &XdgToplevelInterface::windowClassChanged,
            this, &XdgToplevelWindow::handleWindowClassChanged);
    connect(shellSurface, &XdgToplevelInterface::windowMenuRequested,
            this, &XdgToplevelWindow::handleWindowMenuRequested);
    connect(shellSurface, &XdgToplevelInterface::moveRequested,
            this, &XdgToplevelWindow::handleMoveRequested);
    connect(shellSurface, &XdgToplevelInterface::resizeRequested,
            this, &XdgToplevelWindow::handleResizeRequested);
    connect(shellSurface, &XdgToplevelInterface::maximizeRequested,
            this, &XdgToplevelWindow::handleMaximizeRequested);
    connect(shellSurface, &XdgToplevelInterface::unmaximizeRequested,
            this, &XdgToplevelWindow::handleUnmaximizeRequested);
    connect(shellSurface, &XdgToplevelInterface::fullscreenRequested,
            this, &XdgToplevelWindow::handleFullscreenRequested);
    connect(shellSurface, &XdgToplevelInterface::unfullscreenRequested,
            this, &XdgToplevelWindow::handleUnfullscreenRequested);
    connect(shellSurface, &XdgToplevelInterface::minimizeRequested,
            this, &XdgToplevelWindow::handleMinimizeRequested);
    connect(shellSurface, &XdgToplevelInterface::parentXdgToplevelChanged,
            this, &XdgToplevelWindow::handleTransientForChanged);
    connect(shellSurface, &XdgToplevelInterface::initializeRequested,
            this, &XdgToplevelWindow::initialize);
    connect(shellSurface, &XdgToplevelInterface::aboutToBeDestroyed,
            this, &XdgToplevelWindow::destroyWindow);
    connect(shellSurface, &XdgToplevelInterface::maximumSizeChanged,
            this, &XdgToplevelWindow::handleMaximumSizeChanged);
    connect(shellSurface, &XdgToplevelInterface::minimumSizeChanged,
            this, &XdgToplevelWindow::handleMinimumSizeChanged);
    connect(shellSurface->shell(), &XdgShellInterface::pingTimeout,
            this, &XdgToplevelWindow::handlePingTimeout);
    connect(shellSurface->shell(), &XdgShellInterface::pingDelayed,
            this, &XdgToplevelWindow::handlePingDelayed);
    connect(shellSurface->shell(), &XdgShellInterface::pongReceived,
            this, &XdgToplevelWindow::handlePongReceived);

    connect(waylandServer(), &WaylandServer::foreignTransientChanged,
            this, &XdgToplevelWindow::handleForeignTransientForChanged);

    connect(shellSurface, &XdgToplevelInterface::customIconChanged, this, &XdgToplevelWindow::updateIcon);
    connect(this, &XdgToplevelWindow::desktopFileNameChanged, this, &XdgToplevelWindow::updateIcon);
    updateIcon();
}

XdgToplevelWindow::~XdgToplevelWindow()
{
    if (m_killPrompt) {
        m_killPrompt->quit();
    }
}

void XdgToplevelWindow::handleRoleDestroyed()
{
    destroyWindowManagementInterface();

    if (m_appMenuInterface) {
        m_appMenuInterface->disconnect(this);
    }
    if (m_paletteInterface) {
        m_paletteInterface->disconnect(this);
    }
    if (m_xdgDecoration) {
        m_xdgDecoration->disconnect(this);
    }
    if (m_serverDecoration) {
        m_serverDecoration->disconnect(this);
    }
    if (m_xdgDialog) {
        m_xdgDialog->disconnect(this);
    }

    m_shellSurface->disconnect(this);

    disconnect(waylandServer(), &WaylandServer::foreignTransientChanged,
               this, &XdgToplevelWindow::handleForeignTransientForChanged);

    XdgSurfaceWindow::handleRoleDestroyed();
}

XdgToplevelInterface *XdgToplevelWindow::shellSurface() const
{
    return m_shellSurface;
}

MaximizeMode XdgToplevelWindow::maximizeMode() const
{
    return m_maximizeMode;
}

MaximizeMode XdgToplevelWindow::requestedMaximizeMode() const
{
    return m_requestedMaximizeMode;
}

QSizeF XdgToplevelWindow::minSize() const
{
    const int enforcedMinimum = m_nextDecoration ? 150 : 20;
    return rules()->checkMinSize(m_minimumSize).expandedTo(QSizeF(enforcedMinimum, enforcedMinimum));
}

QSizeF XdgToplevelWindow::maxSize() const
{
    // enforce the same minimum as for minSize, so that maxSize is always bigger than minSize
    const int enforcedMinimum = m_nextDecoration ? 150 : 20;
    return rules()->checkMaxSize(QSizeF(m_maximumSize.width() > 0 ? m_maximumSize.width() : INT_MAX,
                                        m_maximumSize.height() > 0 ? m_maximumSize.height() : INT_MAX))
        .expandedTo(QSizeF(enforcedMinimum, enforcedMinimum));
}

bool XdgToplevelWindow::isFullScreen() const
{
    return m_isFullScreen;
}

bool XdgToplevelWindow::isRequestedFullScreen() const
{
    return m_isRequestedFullScreen;
}

bool XdgToplevelWindow::isMovable() const
{
    if (isRequestedFullScreen()) {
        return false;
    }
    if (isSpecialWindow() && !isSplash() && !isToolbar()) {
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }
    if (!options->interactiveWindowMoveEnabled()) {
        return false;
    }
    return true;
}

bool XdgToplevelWindow::isMovableAcrossScreens() const
{
    if (isSpecialWindow() && !isSplash() && !isToolbar()) {
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }
    if (!options->interactiveWindowMoveEnabled()) {
        return false;
    }
    return true;
}

bool XdgToplevelWindow::isResizable() const
{
    if (isRequestedFullScreen()) {
        return false;
    }
    if (isSpecialWindow() && !isAppletPopup()) {
        return false;
    }
    if (rules()->checkSize(QSize()).isValid()) {
        return false;
    }
    const QSizeF min = minSize();
    const QSizeF max = maxSize();
    return min.width() < max.width() || min.height() < max.height();
}

bool XdgToplevelWindow::isCloseable() const
{
    return rules()->checkCloseable(!isDesktop() && !isDock());
}

bool XdgToplevelWindow::isFullScreenable() const
{
    if (!rules()->checkFullScreen(true)) {
        return false;
    }
    return !isSpecialWindow();
}

bool XdgToplevelWindow::isMaximizable() const
{
    if (!isResizable() || isAppletPopup()) {
        return false;
    }
    if (rules()->checkMaximize(MaximizeRestore) != MaximizeRestore || rules()->checkMaximize(MaximizeFull) != MaximizeFull) {
        return false;
    }
    return true;
}

bool XdgToplevelWindow::isMinimizable() const
{
    if ((isSpecialWindow() && !isTransient()) || isAppletPopup()) {
        return false;
    }
    if (!rules()->checkMinimize(true)) {
        return false;
    }
    return true;
}

bool XdgToplevelWindow::isPlaceable() const
{
    if (m_plasmaShellSurface) {
        return !m_plasmaShellSurface->isPositionSet() && !m_plasmaShellSurface->wantsOpenUnderCursor();
    }
    return true;
}

bool XdgToplevelWindow::isTransient() const
{
    return m_isTransient;
}

bool XdgToplevelWindow::userCanSetNoBorder() const
{
    return (m_serverDecoration || m_xdgDecoration) && !isFullScreen() && !isShade();
}

bool XdgToplevelWindow::noBorder() const
{
    return m_userNoBorder;
}

void XdgToplevelWindow::setNoBorder(bool set)
{
    set = rules()->checkNoBorder(set);
    if (m_userNoBorder == set) {
        return;
    }
    m_userNoBorder = set;
    configureDecoration();
    updateWindowRules(Rules::NoBorder);
    Q_EMIT noBorderChanged();
}

KDecoration3::Decoration *XdgToplevelWindow::nextDecoration() const
{
    return m_nextDecoration.get();
}

void XdgToplevelWindow::invalidateDecoration()
{
    clearDecoration();
    configureDecoration();
}

bool XdgToplevelWindow::supportsWindowRules() const
{
    return true;
}

void XdgToplevelWindow::applyWindowRules()
{
    WaylandWindow::applyWindowRules();
    updateCapabilities();
}

void XdgToplevelWindow::closeWindow()
{
    if (isDeleted()) {
        return;
    }
    if (isCloseable()) {
        sendPing(PingReason::CloseWindow);
        m_shellSurface->sendClose();
    }
}

XdgSurfaceConfigure *XdgToplevelWindow::sendRoleConfigure() const
{
    surface()->setPreferredBufferScale(nextTargetScale());
    surface()->setPreferredBufferTransform(preferredBufferTransform());
    surface()->setPreferredColorDescription(preferredColorDescription());

    QSizeF framePadding(0, 0);
    if (m_nextDecoration) {
        const auto borders = m_nextDecorationState->borders();
        framePadding.setWidth(borders.left() + borders.right());
        framePadding.setHeight(borders.top() + borders.bottom());
    }

    QSizeF nextClientSize = snapToPixels(moveResizeGeometry().size(), nextTargetScale());
    if (!nextClientSize.isEmpty()) {
        nextClientSize.setWidth(std::max(1.0, nextClientSize.width() - framePadding.width()));
        nextClientSize.setHeight(std::max(1.0, nextClientSize.height() - framePadding.height()));
    }

    if (nextClientSize.isEmpty()) {
        QSizeF bounds = workspace()->clientArea(PlacementArea, this, moveResizeOutput()).size();
        bounds.setWidth(std::max(1.0, bounds.width() - framePadding.width()));
        bounds.setHeight(std::max(1.0, bounds.height() - framePadding.height()));
        m_shellSurface->sendConfigureBounds(bounds.toSize());
    }

    const quint32 serial = m_shellSurface->sendConfigure(nextClientSize.toSize(), m_nextStates);

    XdgToplevelConfigure *configureEvent = new XdgToplevelConfigure();
    configureEvent->bounds = moveResizeGeometry();
    configureEvent->states = m_nextStates;
    configureEvent->decoration = m_nextDecoration;
    configureEvent->decorationState = m_nextDecorationState;
    configureEvent->serial = serial;
    configureEvent->tile = m_requestedTile;

    return configureEvent;
}

void XdgToplevelWindow::handleRolePrecommit()
{
    if (auto configureEvent = static_cast<XdgToplevelConfigure *>(lastAcknowledgedConfigure())) {
        if (configureEvent->decoration) {
            configureEvent->decoration->apply(configureEvent->decorationState);
        }

        if (decoration() != configureEvent->decoration.get()) {
            setDecoration(configureEvent->decoration);
            updateShadow();
        }
    }
}

void XdgToplevelWindow::handleRoleCommit()
{
    auto configureEvent = static_cast<XdgToplevelConfigure *>(lastAcknowledgedConfigure());
    if (configureEvent) {
        handleStatesAcknowledged(configureEvent->states);
        commitTile(configureEvent->tile);
    }
}

void XdgToplevelWindow::doMinimize()
{
    if (m_isInitialized) {
        if (isMinimized()) {
            workspace()->activateNextWindow(this);
        }
    }
    workspace()->updateMinimizedOfTransients(this);
}

void XdgToplevelWindow::doInteractiveResizeSync(const QRectF &rect)
{
    moveResize(rect);
}

void XdgToplevelWindow::doSetActive()
{
    WaylandWindow::doSetActive();

    if (isActive()) {
        m_nextStates |= XdgToplevelInterface::State::Activated;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::Activated;
    }

    scheduleConfigure();
}

void XdgToplevelWindow::doSetFullScreen()
{
    if (isRequestedFullScreen()) {
        m_nextStates |= XdgToplevelInterface::State::FullScreen;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::FullScreen;
    }

    scheduleConfigure();
}

void XdgToplevelWindow::doSetMaximized()
{
    if (requestedMaximizeMode() & MaximizeHorizontal) {
        m_nextStates |= XdgToplevelInterface::State::MaximizedHorizontal;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::MaximizedHorizontal;
    }

    if (requestedMaximizeMode() & MaximizeVertical) {
        m_nextStates |= XdgToplevelInterface::State::MaximizedVertical;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::MaximizedVertical;
    }

    scheduleConfigure();
}

void XdgToplevelWindow::doSetQuickTileMode()
{
    const Qt::Edges anchors = requestedTile() ? requestedTile()->anchors() : Qt::Edges();

    if (anchors & Qt::LeftEdge) {
        m_nextStates |= XdgToplevelInterface::State::TiledLeft;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::TiledLeft;
    }

    if (anchors & Qt::RightEdge) {
        m_nextStates |= XdgToplevelInterface::State::TiledRight;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::TiledRight;
    }

    if (anchors & Qt::TopEdge) {
        m_nextStates |= XdgToplevelInterface::State::TiledTop;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::TiledTop;
    }

    if (anchors & Qt::BottomEdge) {
        m_nextStates |= XdgToplevelInterface::State::TiledBottom;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::TiledBottom;
    }

    scheduleConfigure();
}

bool XdgToplevelWindow::doStartInteractiveMoveResize()
{
    if (interactiveMoveResizeGravity() != Gravity::None) {
        m_nextGravity = interactiveMoveResizeGravity();
        m_nextStates |= XdgToplevelInterface::State::Resizing;
        scheduleConfigure();
    }
    return true;
}

void XdgToplevelWindow::doFinishInteractiveMoveResize()
{
    if (m_nextStates & XdgToplevelInterface::State::Resizing) {
        m_nextStates &= ~XdgToplevelInterface::State::Resizing;
        scheduleConfigure();
    }
}

void XdgToplevelWindow::doSetSuspended()
{
    if (isSuspended()) {
        m_nextStates |= XdgToplevelInterface::State::Suspended;
    } else {
        m_nextStates &= ~XdgToplevelInterface::State::Suspended;
    }

    scheduleConfigure();
}

void XdgToplevelWindow::doSetNextTargetScale()
{
    if (isDeleted()) {
        return;
    }
    if (m_isInitialized) {
        scheduleConfigure();
    }
}

void XdgToplevelWindow::doSetPreferredBufferTransform()
{
    if (isDeleted()) {
        return;
    }
    if (m_isInitialized) {
        scheduleConfigure();
    }
}

void XdgToplevelWindow::doSetPreferredColorDescription()
{
    if (isDeleted()) {
        return;
    }
    if (m_isInitialized) {
        scheduleConfigure();
    }
}

bool XdgToplevelWindow::takeFocus()
{
    if (wantsInput()) {
        sendPing(PingReason::FocusWindow);
        setActive(true);
    }
    return true;
}

bool XdgToplevelWindow::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus());
}

bool XdgToplevelWindow::dockWantsInput() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Panel) {
            return m_plasmaShellSurface->panelTakesFocus();
        }
    }
    return false;
}

bool XdgToplevelWindow::acceptsFocus() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::OnScreenDisplay || m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::ToolTip) {
            return false;
        }
        switch (m_plasmaShellSurface->role()) {
        case PlasmaShellSurfaceInterface::Role::Notification:
        case PlasmaShellSurfaceInterface::Role::CriticalNotification:
            return m_plasmaShellSurface->panelTakesFocus();
        default:
            break;
        }
    }
    return !isDeleted() && readyForPainting();
}

void XdgToplevelWindow::handleWindowTitleChanged()
{
    setCaption(m_shellSurface->windowTitle());
}

void XdgToplevelWindow::handleWindowClassChanged()
{
    const QString applicationId = m_shellSurface->windowClass();
    setResourceClass(resourceName(), applicationId);
    if (shellSurface()->isConfigured()) {
        evaluateWindowRules();
    }
    setDesktopFileName(applicationId);
}

void XdgToplevelWindow::handleWindowMenuRequested(SeatInterface *seat, const QPoint &surfacePos,
                                                  quint32 serial)
{
    performMousePressCommand(Options::MouseOperationsMenu, mapFromLocal(surfacePos));
}

void XdgToplevelWindow::handleMoveRequested(SeatInterface *seat, quint32 serial)
{
    if (!seat->hasImplicitPointerGrab(serial) && !seat->hasImplicitTouchGrab(serial)
        && !waylandServer()->tabletManagerV2()->seat(seat)->hasImplicitGrab(serial)) {
        return;
    }
    if (isMovable()) {
        QPointF cursorPos;
        if (seat->hasImplicitPointerGrab(serial)) {
            cursorPos = input()->pointer()->pos();
        } else if (seat->hasImplicitTouchGrab(serial)) {
            cursorPos = input()->touch()->position();
        } else {
            cursorPos = input()->tablet()->position();
        }
        performMousePressCommand(Options::MouseMove, cursorPos);
    } else {
        qCDebug(KWIN_CORE) << this << "is immovable, ignoring the move request";
    }
}

void XdgToplevelWindow::handleResizeRequested(SeatInterface *seat, XdgToplevelInterface::ResizeAnchor anchor, quint32 serial)
{
    if (!seat->hasImplicitPointerGrab(serial) && !seat->hasImplicitTouchGrab(serial)
        && !waylandServer()->tabletManagerV2()->seat(seat)->hasImplicitGrab(serial)) {
        return;
    }
    if (!isResizable() || isShade()) {
        return;
    }
    if (isInteractiveMoveResize()) {
        finishInteractiveMoveResize(false);
    }
    setInteractiveMoveResizePointerButtonDown(true);
    QPointF cursorPos;
    if (seat->hasImplicitPointerGrab(serial)) {
        cursorPos = input()->pointer()->pos();
    } else if (seat->hasImplicitTouchGrab(serial)) {
        cursorPos = input()->touch()->position();
    } else {
        cursorPos = input()->tablet()->position();
    }
    setInteractiveMoveResizeAnchor(cursorPos);
    setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers());
    setInteractiveMoveOffset(QPointF((cursorPos.x() - x()) / width(), (cursorPos.y() - y()) / height())); // map from global
    setUnrestrictedInteractiveMoveResize(false);
    Gravity gravity;
    switch (anchor) {
    case XdgToplevelInterface::ResizeAnchor::TopLeft:
        gravity = Gravity::TopLeft;
        break;
    case XdgToplevelInterface::ResizeAnchor::Top:
        gravity = Gravity::Top;
        break;
    case XdgToplevelInterface::ResizeAnchor::TopRight:
        gravity = Gravity::TopRight;
        break;
    case XdgToplevelInterface::ResizeAnchor::Right:
        gravity = Gravity::Right;
        break;
    case XdgToplevelInterface::ResizeAnchor::BottomRight:
        gravity = Gravity::BottomRight;
        break;
    case XdgToplevelInterface::ResizeAnchor::Bottom:
        gravity = Gravity::Bottom;
        break;
    case XdgToplevelInterface::ResizeAnchor::BottomLeft:
        gravity = Gravity::BottomLeft;
        break;
    case XdgToplevelInterface::ResizeAnchor::Left:
        gravity = Gravity::Left;
        break;
    default:
        gravity = Gravity::None;
        break;
    }
    setInteractiveMoveResizeGravity(gravity);
    if (!startInteractiveMoveResize()) {
        setInteractiveMoveResizePointerButtonDown(false);
    }
    updateCursor();
}

void XdgToplevelWindow::handleStatesAcknowledged(const XdgToplevelInterface::States &states)
{
    const XdgToplevelInterface::States delta = m_acknowledgedStates ^ states;

    if (delta & XdgToplevelInterface::State::Maximized) {
        MaximizeMode maximizeMode = MaximizeRestore;
        if (states & XdgToplevelInterface::State::MaximizedHorizontal) {
            maximizeMode = MaximizeMode(maximizeMode | MaximizeHorizontal);
        }
        if (states & XdgToplevelInterface::State::MaximizedVertical) {
            maximizeMode = MaximizeMode(maximizeMode | MaximizeVertical);
        }
        updateMaximizeMode(maximizeMode);
    }
    if (delta & XdgToplevelInterface::State::FullScreen) {
        updateFullScreenMode(states & XdgToplevelInterface::State::FullScreen);
    }

    m_acknowledgedStates = states;
}

void XdgToplevelWindow::handleMaximizeRequested()
{
    if (m_isInitialized) {
        maximize(MaximizeFull);
        scheduleConfigure();
    } else {
        m_initialStates |= XdgToplevelInterface::State::Maximized;
    }
}

void XdgToplevelWindow::handleUnmaximizeRequested()
{
    if (m_isInitialized) {
        maximize(MaximizeRestore);
        scheduleConfigure();
    } else {
        m_initialStates &= ~XdgToplevelInterface::State::Maximized;
    }
}

void XdgToplevelWindow::handleFullscreenRequested(OutputInterface *output)
{
    m_fullScreenRequestedOutput = output ? output->handle() : nullptr;
    if (m_isInitialized) {
        setFullScreen(true);
        scheduleConfigure();
    } else {
        m_initialStates |= XdgToplevelInterface::State::FullScreen;
    }
}

void XdgToplevelWindow::handleUnfullscreenRequested()
{
    m_fullScreenRequestedOutput.clear();
    if (m_isInitialized) {
        setFullScreen(false);
        scheduleConfigure();
    } else {
        m_initialStates &= ~XdgToplevelInterface::State::FullScreen;
    }
}

void XdgToplevelWindow::handleMinimizeRequested()
{
    setMinimized(true);
}

void XdgToplevelWindow::handleTransientForChanged()
{
    SurfaceInterface *transientForSurface = nullptr;
    if (XdgToplevelInterface *parentToplevel = m_shellSurface->parentXdgToplevel()) {
        transientForSurface = parentToplevel->surface();
    }
    if (!transientForSurface) {
        transientForSurface = waylandServer()->findForeignTransientForSurface(surface());
    }
    Window *transientForWindow = waylandServer()->findWindow(transientForSurface);
    if (transientForWindow != transientFor()) {
        if (transientFor()) {
            transientFor()->removeTransient(this);
        }
        if (transientForWindow) {
            transientForWindow->addTransient(this);
        }
        setTransientFor(transientForWindow);
    }
    m_isTransient = transientForWindow;
}

void XdgToplevelWindow::handleForeignTransientForChanged(SurfaceInterface *child)
{
    if (surface() == child) {
        handleTransientForChanged();
    }
}

void XdgToplevelWindow::handlePingTimeout(quint32 serial)
{
    auto pingIt = m_pings.find(serial);
    if (pingIt == m_pings.end()) {
        return;
    }
    if (pingIt.value() == PingReason::CloseWindow) {
        qCDebug(KWIN_CORE) << "Final ping timeout on a close attempt, asking to kill:" << caption();

        if (!m_killPrompt) {
            m_killPrompt = std::make_unique<KillPrompt>(this);
        }
        if (!m_killPrompt->isRunning()) {
            m_killPrompt->start();
        }
    }
}

void XdgToplevelWindow::handlePingDelayed(quint32 serial)
{
    auto it = m_pings.find(serial);
    if (it != m_pings.end()) {
        qCDebug(KWIN_CORE) << "First ping timeout:" << caption();
        setUnresponsive(true);
    }
}

void XdgToplevelWindow::handlePongReceived(quint32 serial)
{
    if (m_pings.remove(serial)) {
        setUnresponsive(false);
        if (m_killPrompt) {
            m_killPrompt->quit();
        }
    }
}

void XdgToplevelWindow::handleMaximumSizeChanged()
{
    m_maximumSize = m_shellSurface->maximumSize();
    updateCapabilities();
    Q_EMIT maximizeableChanged(isMaximizable());
}

void XdgToplevelWindow::handleMinimumSizeChanged()
{
    m_minimumSize = m_shellSurface->minimumSize();
    updateCapabilities();
    Q_EMIT maximizeableChanged(isMaximizable());
}

void XdgToplevelWindow::sendPing(PingReason reason)
{
    XdgShellInterface *shell = m_shellSurface->shell();
    XdgSurfaceInterface *surface = m_shellSurface->xdgSurface();

    const quint32 serial = shell->ping(surface);
    m_pings.insert(serial, reason);
}

MaximizeMode XdgToplevelWindow::initialMaximizeMode() const
{
    MaximizeMode maximizeMode = MaximizeRestore;
    if (m_initialStates & XdgToplevelInterface::State::MaximizedHorizontal) {
        maximizeMode = MaximizeMode(maximizeMode | MaximizeHorizontal);
    }
    if (m_initialStates & XdgToplevelInterface::State::MaximizedVertical) {
        maximizeMode = MaximizeMode(maximizeMode | MaximizeVertical);
    }
    return maximizeMode;
}

bool XdgToplevelWindow::initialFullScreenMode() const
{
    return m_initialStates & XdgToplevelInterface::State::FullScreen;
}

void XdgToplevelWindow::initialize()
{
    bool needsPlacement = isPlaceable();
    setupWindowRules();

    // Move or resize the window only if enforced by a window rule.
    const QPointF forcedPosition = rules()->checkPositionSafe(invalidPoint, true);
    if (forcedPosition != invalidPoint) {
        move(forcedPosition);
    }
    const QSizeF forcedSize = rules()->checkSize(QSize(), true);
    if (forcedSize.isValid()) {
        resize(forcedSize);
    }

    maximize(rules()->checkMaximize(initialMaximizeMode(), true));
    setFullScreen(rules()->checkFullScreen(initialFullScreenMode(), true));
    setOnActivities(rules()->checkActivity(activities(), true));
    setDesktops(rules()->checkDesktops(desktops(), true));
    setDesktopFileName(rules()->checkDesktopFile(desktopFileName(), true));
    setMinimized(rules()->checkMinimize(isMinimized(), true));
    setSkipTaskbar(rules()->checkSkipTaskbar(skipTaskbar(), true));
    setSkipPager(rules()->checkSkipPager(skipPager(), true));
    setSkipSwitcher(rules()->checkSkipSwitcher(skipSwitcher(), true));
    setKeepAbove(rules()->checkKeepAbove(keepAbove(), true));
    setKeepBelow(rules()->checkKeepBelow(keepBelow(), true));
    setShortcut(rules()->checkShortcut(shortcut().toString(), true));
    setNoBorder(rules()->checkNoBorder(noBorder(), true));

    // Don't place the client if its position is set by a rule.
    if (rules()->checkPosition(invalidPoint, true) != invalidPoint) {
        needsPlacement = false;
    }

    // Don't place the client if the maximize state is set by a rule.
    if (requestedMaximizeMode() != MaximizeRestore) {
        needsPlacement = false;
    }

    workspace()->rulebook()->discardUsed(this, false); // Remove Apply Now rules.
    updateWindowRules(Rules::All);

    if (isRequestedFullScreen()) {
        needsPlacement = false;
    }
    if (needsPlacement) {
        const QRectF area = workspace()->clientArea(PlacementArea, this, workspace()->activeOutput());
        workspace()->placement()->place(this, area);
    }

    configureDecoration();
    scheduleConfigure();
    updateColorScheme();
    updateCapabilities();
    setupWindowManagementInterface();

    m_isInitialized = true;
}

void XdgToplevelWindow::updateMaximizeMode(MaximizeMode maximizeMode)
{
    if (m_maximizeMode == maximizeMode) {
        return;
    }
    m_maximizeMode = maximizeMode;
    updateWindowRules(Rules::MaximizeVert | Rules::MaximizeHoriz);
    Q_EMIT maximizedChanged();
}

void XdgToplevelWindow::updateFullScreenMode(bool set)
{
    if (m_isFullScreen == set) {
        return;
    }
    StackingUpdatesBlocker blocker1(workspace());
    m_isFullScreen = set;
    updateLayer();
    updateWindowRules(Rules::Fullscreen);
    Q_EMIT fullScreenChanged();
}

void XdgToplevelWindow::updateCapabilities()
{
    XdgToplevelInterface::Capabilities caps = XdgToplevelInterface::Capability::WindowMenu;

    if (isMaximizable()) {
        caps.setFlag(XdgToplevelInterface::Capability::Maximize);
    }
    if (isFullScreenable()) {
        caps.setFlag(XdgToplevelInterface::Capability::FullScreen);
    }
    if (isMinimizable()) {
        caps.setFlag(XdgToplevelInterface::Capability::Minimize);
    }

    if (m_capabilities != caps) {
        m_capabilities = caps;
        m_shellSurface->sendWmCapabilities(caps);
    }
}

void XdgToplevelWindow::updateIcon()
{
    if (!m_shellSurface->customIcon().isNull()) {
        setIcon(m_shellSurface->customIcon());
        return;
    }
    const QString waylandIconName = QStringLiteral("wayland");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

QString XdgToplevelWindow::preferredColorScheme() const
{
    if (m_paletteInterface) {
        return rules()->checkDecoColor(m_paletteInterface->palette());
    }
    return rules()->checkDecoColor(QString());
}

void XdgToplevelWindow::installAppMenu(AppMenuInterface *appMenu)
{
    m_appMenuInterface = appMenu;

    auto updateMenu = [this](const AppMenuInterface::InterfaceAddress &address) {
        updateApplicationMenuServiceName(address.serviceName);
        updateApplicationMenuObjectPath(address.objectPath);
    };
    connect(m_appMenuInterface, &AppMenuInterface::addressChanged, this, updateMenu);
    updateMenu(appMenu->address());
}

XdgToplevelWindow::DecorationMode XdgToplevelWindow::preferredDecorationMode() const
{
    if (!Decoration::DecorationBridge::hasPlugin()) {
        return DecorationMode::Client;
    } else if (m_userNoBorder || isRequestedFullScreen()) {
        return DecorationMode::None;
    }

    if (m_xdgDecoration) {
        switch (m_xdgDecoration->preferredMode()) {
        case XdgToplevelDecorationV1Interface::Mode::Undefined:
            return DecorationMode::Server;
        case XdgToplevelDecorationV1Interface::Mode::None:
            return DecorationMode::None;
        case XdgToplevelDecorationV1Interface::Mode::Client:
            return DecorationMode::Client;
        case XdgToplevelDecorationV1Interface::Mode::Server:
            return DecorationMode::Server;
        }
    }

    if (m_serverDecoration) {
        switch (m_serverDecoration->preferredMode()) {
        case ServerSideDecorationManagerInterface::Mode::None:
            return DecorationMode::None;
        case ServerSideDecorationManagerInterface::Mode::Client:
            return DecorationMode::Client;
        case ServerSideDecorationManagerInterface::Mode::Server:
            return DecorationMode::Server;
        }
    }

    return DecorationMode::Client;
}

void XdgToplevelWindow::clearDecoration()
{
    if (m_nextDecoration) {
        disconnect(m_nextDecoration.get(), &KDecoration3::Decoration::nextStateChanged, this, &XdgToplevelWindow::processDecorationState);
    }

    m_nextDecoration = nullptr;
    m_nextDecorationState = nullptr;
}

void XdgToplevelWindow::configureDecoration()
{
    const DecorationMode decorationMode = preferredDecorationMode();
    switch (decorationMode) {
    case DecorationMode::None:
    case DecorationMode::Client:
        clearDecoration();
        break;
    case DecorationMode::Server:
        if (!m_nextDecoration) {
            m_nextDecoration.reset(Workspace::self()->decorationBridge()->createDecoration(this));
            if (m_nextDecoration) {
                connect(m_nextDecoration.get(), &KDecoration3::Decoration::nextStateChanged, this, &XdgToplevelWindow::processDecorationState);
                m_nextDecorationState = m_nextDecoration->nextState()->clone();
            }
        }
        break;
    }

    // All decoration updates are synchronized to toplevel configure events.
    if (m_xdgDecoration) {
        configureXdgDecoration(decorationMode);
    } else if (m_serverDecoration) {
        configureServerDecoration(decorationMode);
    }
}

void XdgToplevelWindow::processDecorationState(std::shared_ptr<KDecoration3::DecorationState> state)
{
    if (isDeleted()) {
        return;
    }

    m_nextDecorationState = state->clone();
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

void XdgToplevelWindow::configureXdgDecoration(DecorationMode decorationMode)
{
    switch (decorationMode) {
    case DecorationMode::None: // Faked as server side mode under the hood.
        m_xdgDecoration->sendConfigure(XdgToplevelDecorationV1Interface::Mode::None);
        break;
    case DecorationMode::Client:
        m_xdgDecoration->sendConfigure(XdgToplevelDecorationV1Interface::Mode::Client);
        break;
    case DecorationMode::Server:
        m_xdgDecoration->sendConfigure(XdgToplevelDecorationV1Interface::Mode::Server);
        break;
    }
    scheduleConfigure();
}

void XdgToplevelWindow::configureServerDecoration(DecorationMode decorationMode)
{
    switch (decorationMode) {
    case DecorationMode::None:
        m_serverDecoration->setMode(ServerSideDecorationManagerInterface::Mode::None);
        break;
    case DecorationMode::Client:
        m_serverDecoration->setMode(ServerSideDecorationManagerInterface::Mode::Client);
        break;
    case DecorationMode::Server:
        m_serverDecoration->setMode(ServerSideDecorationManagerInterface::Mode::Server);
        break;
    }
    scheduleConfigure();
}

void XdgToplevelWindow::installXdgDecoration(XdgToplevelDecorationV1Interface *decoration)
{
    m_xdgDecoration = decoration;

    connect(m_xdgDecoration, &XdgToplevelDecorationV1Interface::destroyed,
            this, &XdgToplevelWindow::clearDecoration);
    connect(m_xdgDecoration, &XdgToplevelDecorationV1Interface::preferredModeChanged, this, [this] {
        if (m_isInitialized) {
            configureDecoration();
        }
    });
}

void XdgToplevelWindow::installServerDecoration(ServerSideDecorationInterface *decoration)
{
    m_serverDecoration = decoration;
    if (m_isInitialized) {
        configureDecoration();
    }

    connect(m_serverDecoration, &ServerSideDecorationInterface::destroyed,
            this, &XdgToplevelWindow::clearDecoration);
    connect(m_serverDecoration, &ServerSideDecorationInterface::preferredModeChanged, this, [this]() {
        if (m_isInitialized) {
            configureDecoration();
        }
    });
}

void XdgToplevelWindow::installPalette(ServerSideDecorationPaletteInterface *palette)
{
    m_paletteInterface = palette;

    connect(m_paletteInterface, &ServerSideDecorationPaletteInterface::paletteChanged,
            this, &XdgToplevelWindow::updateColorScheme);
    connect(m_paletteInterface, &QObject::destroyed,
            this, &XdgToplevelWindow::updateColorScheme);
    updateColorScheme();
}

void XdgToplevelWindow::installXdgDialogV1(XdgDialogV1Interface *dialog)
{
    m_xdgDialog = dialog;

    connect(dialog, &XdgDialogV1Interface::modalChanged, this, &Window::setModal);
    connect(dialog, &QObject::destroyed, this, [this] {
        setModal(false);
    });
    setModal(dialog->isModal());
}

void XdgToplevelWindow::setFullScreen(bool set)
{
    if (!isFullScreenable()) {
        return;
    }

    set = rules()->checkFullScreen(set);
    if (m_isRequestedFullScreen == set) {
        return;
    }

    m_isRequestedFullScreen = set;
    configureDecoration();

    if (set) {
        const Output *output = m_fullScreenRequestedOutput ? m_fullScreenRequestedOutput.data() : moveResizeOutput();
        setFullscreenGeometryRestore(moveResizeGeometry());
        moveResize(workspace()->clientArea(FullScreenArea, this, output));
    } else {
        m_fullScreenRequestedOutput.clear();
        if (fullscreenGeometryRestore().isValid()) {
            moveResize(QRectF(fullscreenGeometryRestore().topLeft(),
                              constrainFrameSize(fullscreenGeometryRestore().size())));
        } else {
            // this can happen when the window was first shown already fullscreen,
            // so let the client set the size by itself
            moveResize(QRectF(workspace()->clientArea(PlacementArea, this).topLeft(), QSize(0, 0)));
        }
    }

    doSetFullScreen();
}

static bool changeMaximizeRecursion = false;
void XdgToplevelWindow::maximize(MaximizeMode mode, const QRectF &restore)
{
    if (changeMaximizeRecursion) {
        return;
    }

    if (!isResizable() || isAppletPopup()) {
        return;
    }

    const QRectF clientArea = isElectricBorderMaximizing() ? workspace()->clientArea(MaximizeArea, this, interactiveMoveResizeAnchor()) : workspace()->clientArea(MaximizeArea, this, moveResizeOutput());

    const MaximizeMode oldMode = m_requestedMaximizeMode;
    const QRectF oldGeometry = moveResizeGeometry();

    mode = rules()->checkMaximize(mode);
    if (m_requestedMaximizeMode == mode) {
        return;
    }

    Q_EMIT maximizedAboutToChange(mode);
    m_requestedMaximizeMode = mode;

    // call into decoration update borders
    if (m_nextDecoration && !(options->borderlessMaximizedWindows() && m_requestedMaximizeMode == MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = m_nextDecoration->window();
        if ((m_requestedMaximizeMode & MaximizeVertical) != (oldMode & MaximizeVertical)) {
            Q_EMIT c->maximizedVerticallyChanged(m_requestedMaximizeMode & MaximizeVertical);
        }
        if ((m_requestedMaximizeMode & MaximizeHorizontal) != (oldMode & MaximizeHorizontal)) {
            Q_EMIT c->maximizedHorizontallyChanged(m_requestedMaximizeMode & MaximizeHorizontal);
        }
        if ((m_requestedMaximizeMode == MaximizeFull) != (oldMode == MaximizeFull)) {
            Q_EMIT c->maximizedChanged(m_requestedMaximizeMode == MaximizeFull);
        }
        changeMaximizeRecursion = false;
    }

    if (options->borderlessMaximizedWindows()) {
        setNoBorder(m_requestedMaximizeMode == MaximizeFull);
    }

    if (!restore.isNull()) {
        setGeometryRestore(restore);
    } else {
        if (requestedQuickTileMode() == QuickTileMode(QuickTileFlag::None)) {
            QRectF savedGeometry = geometryRestore();
            if (!(oldMode & MaximizeVertical)) {
                savedGeometry.setTop(oldGeometry.top());
                savedGeometry.setBottom(oldGeometry.bottom());
            }
            if (!(oldMode & MaximizeHorizontal)) {
                savedGeometry.setLeft(oldGeometry.left());
                savedGeometry.setRight(oldGeometry.right());
            }
            setGeometryRestore(savedGeometry);
        }
    }

    QRectF geometry = oldGeometry;

    if (m_requestedMaximizeMode & MaximizeHorizontal) {
        // Stretch the window vertically to fit the size of the maximize area.
        geometry.setX(clientArea.x());
        geometry.setWidth(clientArea.width());
    } else if (oldMode & MaximizeHorizontal) {
        if (geometryRestore().isValid()) {
            // The window is no longer maximized horizontally and the saved geometry is valid.
            geometry.setX(geometryRestore().x());
            geometry.setWidth(geometryRestore().width());
        } else {
            // The window is no longer maximized horizontally and the saved geometry is
            // invalid. This would happen if the window had been mapped in the maximized state.
            // We ask the client to resize the window horizontally to its preferred size.
            geometry.setX(clientArea.x());
            geometry.setWidth(0);
        }
    }

    if (m_requestedMaximizeMode & MaximizeVertical) {
        // Stretch the window horizontally to fit the size of the maximize area.
        geometry.setY(clientArea.y());
        geometry.setHeight(clientArea.height());
    } else if (oldMode & MaximizeVertical) {
        if (geometryRestore().isValid()) {
            // The window is no longer maximized vertically and the saved geometry is valid.
            geometry.setY(geometryRestore().y());
            geometry.setHeight(geometryRestore().height());
        } else {
            // The window is no longer maximized vertically and the saved geometry is
            // invalid. This would happen if the window had been mapped in the maximized state.
            // We ask the client to resize the window vertically to its preferred size.
            geometry.setY(clientArea.y());
            geometry.setHeight(0);
        }
    }

    updateQuickTileMode(QuickTileFlag::None);

    moveResize(geometry);

    doSetMaximized();
}

XdgPopupWindow::XdgPopupWindow(XdgPopupInterface *shellSurface)
    : XdgSurfaceWindow(shellSurface->xdgSurface())
    , m_shellSurface(shellSurface)
{
    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());

    m_windowType = WindowType::Unknown;

    connect(shellSurface, &XdgPopupInterface::grabRequested,
            this, &XdgPopupWindow::handleGrabRequested);
    connect(shellSurface, &XdgPopupInterface::initializeRequested,
            this, &XdgPopupWindow::initialize);
    connect(shellSurface, &XdgPopupInterface::repositionRequested,
            this, &XdgPopupWindow::handleRepositionRequested);
    connect(shellSurface, &XdgPopupInterface::aboutToBeDestroyed,
            this, &XdgPopupWindow::destroyWindow);
}

void XdgPopupWindow::handleRoleDestroyed()
{
    if (transientFor()) {
        disconnect(transientFor(), &Window::frameGeometryChanged,
                   this, &XdgPopupWindow::relayout);
        disconnect(transientFor(), &Window::closed,
                   this, &XdgPopupWindow::popupDone);
    }

    m_shellSurface->disconnect(this);

    XdgSurfaceWindow::handleRoleDestroyed();
}

void XdgPopupWindow::handleRepositionRequested(quint32 token)
{
    updateRelativePlacement();
    m_shellSurface->sendRepositioned(token);
    relayout();
}

void XdgPopupWindow::updateRelativePlacement()
{
    const QPointF parentPosition = transientFor()->nextFramePosToClientPos(transientFor()->pos());
    const QRectF bounds = workspace()->clientArea(transientFor()->isFullScreen() ? FullScreenArea : PlacementArea, transientFor()).translated(-parentPosition);
    const XdgPositioner positioner = m_shellSurface->positioner();

    if (m_plasmaShellSurface && m_plasmaShellSurface->isPositionSet()) {
        m_relativePlacement = QRectF(m_plasmaShellSurface->position(), positioner.size()).translated(-parentPosition);
    } else {
        m_relativePlacement = positioner.placement(bounds);
    }
}

void XdgPopupWindow::relayout()
{
    if (m_shellSurface->positioner().isReactive()) {
        updateRelativePlacement();
    }
    workspace()->placement()->place(this, QRectF());
    scheduleConfigure();
}

XdgPopupWindow::~XdgPopupWindow()
{
}

bool XdgPopupWindow::hasPopupGrab() const
{
    return m_haveExplicitGrab;
}

void XdgPopupWindow::popupDone()
{
    m_shellSurface->sendPopupDone();
    destroyWindow();
}

bool XdgPopupWindow::isPopupWindow() const
{
    return true;
}

bool XdgPopupWindow::isTransient() const
{
    return true;
}

bool XdgPopupWindow::isResizable() const
{
    return false;
}

bool XdgPopupWindow::isMovable() const
{
    return false;
}

bool XdgPopupWindow::isMovableAcrossScreens() const
{
    return false;
}

bool XdgPopupWindow::hasTransientPlacementHint() const
{
    return true;
}

QRectF XdgPopupWindow::transientPlacement() const
{
    const QPointF parentPosition = transientFor()->nextFramePosToClientPos(transientFor()->pos());
    return m_relativePlacement.translated(parentPosition);
}

bool XdgPopupWindow::isCloseable() const
{
    return false;
}

void XdgPopupWindow::closeWindow()
{
}

bool XdgPopupWindow::wantsInput() const
{
    return false;
}

bool XdgPopupWindow::takeFocus()
{
    return false;
}

bool XdgPopupWindow::acceptsFocus() const
{
    return false;
}

XdgSurfaceConfigure *XdgPopupWindow::sendRoleConfigure() const
{
    surface()->setPreferredBufferScale(nextTargetScale());
    surface()->setPreferredBufferTransform(preferredBufferTransform());
    surface()->setPreferredColorDescription(preferredColorDescription());

    const quint32 serial = m_shellSurface->sendConfigure(m_relativePlacement.toRect());

    XdgSurfaceConfigure *configureEvent = new XdgSurfaceConfigure();
    configureEvent->bounds = moveResizeGeometry();
    configureEvent->serial = serial;

    return configureEvent;
}

void XdgPopupWindow::handleGrabRequested(SeatInterface *seat, quint32 serial)
{
    m_haveExplicitGrab = true;
}

void XdgPopupWindow::initialize()
{
    Window *parent = waylandServer()->findWindow(m_shellSurface->parentSurface());
    if (!parent) {
        popupDone();
        return;
    }

    parent->addTransient(this);
    setTransientFor(parent);
    setDesktops(parent->desktops());
#if KWIN_BUILD_ACTIVITIES
    setOnActivities(parent->activities());
#endif

    updateRelativePlacement();
    connect(parent, &Window::frameGeometryChanged, this, &XdgPopupWindow::relayout);
    connect(parent, &Window::closed, this, &XdgPopupWindow::popupDone);

    workspace()->placement()->place(this, QRectF());
    scheduleConfigure();
}

void XdgPopupWindow::doSetNextTargetScale()
{
    if (isDeleted()) {
        return;
    }
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

void XdgPopupWindow::doSetPreferredBufferTransform()
{
    if (isDeleted()) {
        return;
    }
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

void XdgPopupWindow::doSetPreferredColorDescription()
{
    if (isDeleted()) {
        return;
    }
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

} // namespace KWin

#include "moc_xdgshellwindow.cpp"
