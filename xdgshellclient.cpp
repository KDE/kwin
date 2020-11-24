/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xdgshellclient.h"
#include "deleted.h"
#include "screenedge.h"
#include "screens.h"
#include "subsurfacemonitor.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KWaylandServer/appmenu_interface.h>
#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/plasmashell_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/server_decoration_interface.h>
#include <KWaylandServer/server_decoration_palette_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/xdgdecoration_v1_interface.h>
#include <KWaylandServer/xdgshell_interface.h>

using namespace KWaylandServer;

namespace KWin
{

XdgSurfaceClient::XdgSurfaceClient(XdgSurfaceInterface *shellSurface)
    : WaylandClient(shellSurface->surface())
    , m_shellSurface(shellSurface)
    , m_configureTimer(new QTimer(this))
{
    setSizeSyncMode(SyncMode::Async);
    setPositionSyncMode(SyncMode::Async);

    connect(shellSurface, &XdgSurfaceInterface::configureAcknowledged,
            this, &XdgSurfaceClient::handleConfigureAcknowledged);
    connect(shellSurface, &XdgSurfaceInterface::resetOccurred,
            this, &XdgSurfaceClient::destroyClient);
    connect(shellSurface->surface(), &SurfaceInterface::committed,
            this, &XdgSurfaceClient::handleCommit);
#if 0 // TODO: Refactor kwin core in order to uncomment this code.
    connect(shellSurface->surface(), &SurfaceInterface::mapped,
            this, &XdgSurfaceClient::setReadyForPainting);
#endif
    connect(shellSurface->surface(), &SurfaceInterface::aboutToBeDestroyed,
            this, &XdgSurfaceClient::destroyClient);

    // The effective window geometry is determined by two things: (a) the rectangle that bounds
    // the main surface and all of its sub-surfaces, (b) the client-specified window geometry, if
    // any. If the client hasn't provided the window geometry, we fallback to the bounding sub-
    // surface rectangle. If the client has provided the window geometry, we intersect it with
    // the bounding rectangle and that will be the effective window geometry. It's worth to point
    // out that geometry updates do not occur that frequently, so we don't need to recompute the
    // bounding geometry every time the client commits the surface.

    SubSurfaceMonitor *treeMonitor = new SubSurfaceMonitor(surface(), this);

    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceAdded,
            this, &XdgSurfaceClient::setHaveNextWindowGeometry);
    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceRemoved,
            this, &XdgSurfaceClient::setHaveNextWindowGeometry);
    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceMoved,
            this, &XdgSurfaceClient::setHaveNextWindowGeometry);
    connect(treeMonitor, &SubSurfaceMonitor::subSurfaceResized,
            this, &XdgSurfaceClient::setHaveNextWindowGeometry);
    connect(shellSurface, &XdgSurfaceInterface::windowGeometryChanged,
            this, &XdgSurfaceClient::setHaveNextWindowGeometry);
    connect(surface(), &SurfaceInterface::sizeChanged,
            this, &XdgSurfaceClient::setHaveNextWindowGeometry);

    // Configure events are not sent immediately, but rather scheduled to be sent when the event
    // loop is about to be idle. By doing this, we can avoid sending configure events that do
    // nothing, and implementation-wise, it's simpler.

    m_configureTimer->setSingleShot(true);
    connect(m_configureTimer, &QTimer::timeout, this, &XdgSurfaceClient::sendConfigure);

    // Unfortunately, AbstractClient::checkWorkspacePosition() operates on the geometry restore
    // so we need to initialize it with some reasonable value; otherwise bad things will happen
    // when we want to decorate the client or move the client to another screen. This is a hack.

    connect(this, &XdgSurfaceClient::frameGeometryChanged,
            this, &XdgSurfaceClient::updateGeometryRestoreHack);
}

XdgSurfaceClient::~XdgSurfaceClient()
{
    qDeleteAll(m_configureEvents);
}

QRect XdgSurfaceClient::inputGeometry() const
{
    return isDecorated() ? AbstractClient::inputGeometry() : bufferGeometry();
}

QMatrix4x4 XdgSurfaceClient::inputTransformation() const
{
    QMatrix4x4 transformation;
    transformation.translate(-bufferGeometry().x(), -bufferGeometry().y());
    return transformation;
}

XdgSurfaceConfigure *XdgSurfaceClient::lastAcknowledgedConfigure() const
{
    return m_lastAcknowledgedConfigure.data();
}

bool XdgSurfaceClient::stateCompare() const
{
    if (requestedFrameGeometry() != frameGeometry()) {
        return true;
    }
    if (requestedClientGeometry() != clientGeometry()) {
        return true;
    }
    if (requestedClientGeometry().isEmpty()) {
        return true;
    }
    return false;
}

void XdgSurfaceClient::scheduleConfigure()
{
    if (isZombie()) {
        return;
    }

    if (stateCompare()) {
        m_configureTimer->start();
    } else {
        m_configureTimer->stop();
    }
}

void XdgSurfaceClient::sendConfigure()
{
    XdgSurfaceConfigure *configureEvent = sendRoleConfigure();

    if (configureEvent->position != pos()) {
        configureEvent->presentFields |= XdgSurfaceConfigure::PositionField;
    }
    if (configureEvent->size != size()) {
        configureEvent->presentFields |= XdgSurfaceConfigure::SizeField;
    }

    m_configureEvents.append(configureEvent);
}

void XdgSurfaceClient::handleConfigureAcknowledged(quint32 serial)
{
    while (!m_configureEvents.isEmpty()) {
        if (serial < m_configureEvents.first()->serial) {
            break;
        }
        m_lastAcknowledgedConfigure.reset(m_configureEvents.takeFirst());
    }
}

void XdgSurfaceClient::handleCommit()
{
    if (!surface()->buffer()) {
        return;
    }

    if (haveNextWindowGeometry()) {
        handleNextWindowGeometry();
        resetHaveNextWindowGeometry();
    }

    handleRoleCommit();
    m_lastAcknowledgedConfigure.reset();

    setReadyForPainting();
    updateDepth();
}

void XdgSurfaceClient::handleRoleCommit()
{
}

void XdgSurfaceClient::handleNextWindowGeometry()
{
    const QRect boundingGeometry = surface()->boundingRect();

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
    }

    QRect frameGeometry(pos(), clientSizeToFrameSize(m_windowGeometry.size()));

    // We're not done yet. The xdg-shell spec allows clients to attach buffers smaller than
    // we asked. Normally, this is not a big deal, but when the client is being interactively
    // resized, it may cause the window contents to bounce. In order to counter this, we have
    // to "gravitate" the new geometry according to the current move-resize pointer mode so
    // the opposite window corner stays still.

    if (isMoveResize()) {
        frameGeometry = adjustMoveResizeGeometry(frameGeometry);
    } else if (lastAcknowledgedConfigure()) {
        XdgSurfaceConfigure *configureEvent = lastAcknowledgedConfigure();

        if (configureEvent->presentFields & XdgSurfaceConfigure::PositionField) {
            frameGeometry.moveTopLeft(configureEvent->position);
        }
    }

    updateGeometry(frameGeometry);

    if (isResize()) {
        performMoveResize();
    }
}

bool XdgSurfaceClient::haveNextWindowGeometry() const
{
    return m_haveNextWindowGeometry || m_lastAcknowledgedConfigure;
}

void XdgSurfaceClient::setHaveNextWindowGeometry()
{
    m_haveNextWindowGeometry = true;
}

void XdgSurfaceClient::resetHaveNextWindowGeometry()
{
    m_haveNextWindowGeometry = false;
}

QRect XdgSurfaceClient::adjustMoveResizeGeometry(const QRect &rect) const
{
    QRect geometry = rect;

    switch (moveResizePointerMode()) {
    case PositionTopLeft:
        geometry.moveRight(moveResizeGeometry().right());
        geometry.moveBottom(moveResizeGeometry().bottom());
        break;
    case PositionTop:
    case PositionTopRight:
        geometry.moveLeft(moveResizeGeometry().left());
        geometry.moveBottom(moveResizeGeometry().bottom());
        break;
    case PositionRight:
    case PositionBottomRight:
    case PositionBottom:
    case PositionCenter:
        geometry.moveLeft(moveResizeGeometry().left());
        geometry.moveTop(moveResizeGeometry().top());
        break;
    case PositionBottomLeft:
    case PositionLeft:
        geometry.moveRight(moveResizeGeometry().right());
        geometry.moveTop(moveResizeGeometry().top());
        break;
    }

    return geometry;
}

bool XdgSurfaceClient::isInitialPositionSet() const
{
    return m_plasmaShellSurface ? m_plasmaShellSurface->isPositionSet() : false;
}

void XdgSurfaceClient::requestGeometry(const QRect &rect)
{
    WaylandClient::requestGeometry(rect);

    scheduleConfigure(); // Send the configure event later.
}

/**
 * \internal
 * \todo We have to check the current frame geometry in checkWorskpacePosition().
 *
 * Sets the geometry restore to the first valid frame geometry. This is a HACK!
 *
 * Unfortunately, AbstractClient::checkWorkspacePosition() operates on the geometry restore
 * rather than the current frame geometry, so we have to ensure that it's initialized with
 * some reasonable value even if the client is not maximized or quick tiled.
 */
void XdgSurfaceClient::updateGeometryRestoreHack()
{
    if (geometryRestore().isEmpty() && !frameGeometry().isEmpty()) {
        setGeometryRestore(frameGeometry());
    }
}

QRect XdgSurfaceClient::frameRectToBufferRect(const QRect &rect) const
{
    const int left = rect.left() + borderLeft() - m_windowGeometry.left();
    const int top = rect.top() + borderTop() - m_windowGeometry.top();
    return QRect(QPoint(left, top), surface()->size());
}

void XdgSurfaceClient::addDamage(const QRegion &damage)
{
    const int offsetX = bufferGeometry().x() - frameGeometry().x();
    const int offsetY = bufferGeometry().y() - frameGeometry().y();
    repaints_region += damage.translated(offsetX, offsetY);
    Toplevel::addDamage(damage);
}

void XdgSurfaceClient::destroyClient()
{
    markAsZombie();
    m_configureTimer->stop();
    if (isMoveResize()) {
        leaveMoveResize();
    }
    cleanTabBox();
    Deleted *deleted = Deleted::create(this);
    emit windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    RuleBook::self()->discardUsed(this, true);
    destroyWindowManagementInterface();
    destroyDecoration();
    cleanGrouping();
    waylandServer()->removeClient(this);
    deleted->unrefWindow();
    delete this;
}

void XdgSurfaceClient::setVirtualKeyboardGeometry(const QRect &geo)
{
    // No keyboard anymore
    if (geo.isEmpty() && !keyboardGeometryRestore().isEmpty()) {
        setFrameGeometry(keyboardGeometryRestore());
        setKeyboardGeometryRestore(QRect());
    } else if (geo.isEmpty()) {
        return;
    // The keyboard has just been opened (rather than resized) save client geometry for a restore
    } else if (keyboardGeometryRestore().isEmpty()) {
        setKeyboardGeometryRestore(requestedFrameGeometry().isEmpty() ? frameGeometry() : requestedFrameGeometry());
    }

    m_virtualKeyboardGeometry = geo;

    // Don't resize Desktop and fullscreen windows
    if (isFullScreen() || isDesktop()) {
        return;
    }

    if (!geo.intersects(keyboardGeometryRestore())) {
        return;
    }

    const QRect availableArea = workspace()->clientArea(MaximizeArea, this);
    QRect newWindowGeometry = keyboardGeometryRestore();
    newWindowGeometry.moveBottom(geo.top());
    newWindowGeometry.setTop(qMax(newWindowGeometry.top(), availableArea.top()));

    setFrameGeometry(newWindowGeometry);
}

XdgToplevelClient::XdgToplevelClient(XdgToplevelInterface *shellSurface)
    : XdgSurfaceClient(shellSurface->xdgSurface())
    , m_shellSurface(shellSurface)
{
    setupWindowManagementIntegration();
    setupPlasmaShellIntegration();
    setDesktop(VirtualDesktopManager::self()->current());

    if (waylandServer()->inputMethodConnection() == surface()->client()) {
        m_windowType = NET::OnScreenDisplay;
    }

    connect(shellSurface, &XdgToplevelInterface::windowTitleChanged,
            this, &XdgToplevelClient::handleWindowTitleChanged);
    connect(shellSurface, &XdgToplevelInterface::windowClassChanged,
            this, &XdgToplevelClient::handleWindowClassChanged);
    connect(shellSurface, &XdgToplevelInterface::windowMenuRequested,
            this, &XdgToplevelClient::handleWindowMenuRequested);
    connect(shellSurface, &XdgToplevelInterface::moveRequested,
            this, &XdgToplevelClient::handleMoveRequested);
    connect(shellSurface, &XdgToplevelInterface::resizeRequested,
            this, &XdgToplevelClient::handleResizeRequested);
    connect(shellSurface, &XdgToplevelInterface::maximizeRequested,
            this, &XdgToplevelClient::handleMaximizeRequested);
    connect(shellSurface, &XdgToplevelInterface::unmaximizeRequested,
            this, &XdgToplevelClient::handleUnmaximizeRequested);
    connect(shellSurface, &XdgToplevelInterface::fullscreenRequested,
            this, &XdgToplevelClient::handleFullscreenRequested);
    connect(shellSurface, &XdgToplevelInterface::unfullscreenRequested,
            this, &XdgToplevelClient::handleUnfullscreenRequested);
    connect(shellSurface, &XdgToplevelInterface::minimizeRequested,
            this, &XdgToplevelClient::handleMinimizeRequested);
    connect(shellSurface, &XdgToplevelInterface::parentXdgToplevelChanged,
            this, &XdgToplevelClient::handleTransientForChanged);
    connect(shellSurface, &XdgToplevelInterface::initializeRequested,
            this, &XdgToplevelClient::initialize);
    connect(shellSurface, &XdgToplevelInterface::destroyed,
            this, &XdgToplevelClient::destroyClient);
    connect(shellSurface->shell(), &XdgShellInterface::pingTimeout,
            this, &XdgToplevelClient::handlePingTimeout);
    connect(shellSurface->shell(), &XdgShellInterface::pingDelayed,
            this, &XdgToplevelClient::handlePingDelayed);
    connect(shellSurface->shell(), &XdgShellInterface::pongReceived,
            this, &XdgToplevelClient::handlePongReceived);

    connect(waylandServer(), &WaylandServer::foreignTransientChanged,
            this, &XdgToplevelClient::handleForeignTransientForChanged);
}

XdgToplevelClient::~XdgToplevelClient()
{
}

XdgToplevelInterface *XdgToplevelClient::shellSurface() const
{
    return m_shellSurface;
}

NET::WindowType XdgToplevelClient::windowType(bool direct, int supported_types) const
{
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

MaximizeMode XdgToplevelClient::maximizeMode() const
{
    return m_maximizeMode;
}

MaximizeMode XdgToplevelClient::requestedMaximizeMode() const
{
    return m_requestedMaximizeMode;
}

QSize XdgToplevelClient::minSize() const
{
    return rules()->checkMinSize(m_shellSurface->minimumSize());
}

QSize XdgToplevelClient::maxSize() const
{
    return rules()->checkMaxSize(m_shellSurface->maximumSize());
}

bool XdgToplevelClient::isFullScreen() const
{
    return m_isFullScreen;
}

bool XdgToplevelClient::isMovable() const
{
    if (isFullScreen()) {
        return false;
    }
    if (isSpecialWindow() && !isSplash() && !isToolbar()) {
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }
    return true;
}

bool XdgToplevelClient::isMovableAcrossScreens() const
{
    if (isSpecialWindow() && !isSplash() && !isToolbar()) {
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }
    return true;
}

bool XdgToplevelClient::isResizable() const
{
    if (isFullScreen()) {
        return false;
    }
    if (isSpecialWindow() || isSplash() || isToolbar()) {
        return false;
    }
    if (rules()->checkSize(QSize()).isValid()) {
        return false;
    }
    const QSize min = minSize();
    const QSize max = maxSize();
    return min.width() < max.width() || min.height() < max.height();
}

bool XdgToplevelClient::isCloseable() const
{
    return !isDesktop() && !isDock();
}

bool XdgToplevelClient::isFullScreenable() const
{
    if (!rules()->checkFullScreen(true)) {
        return false;
    }
    return !isSpecialWindow();
}

bool XdgToplevelClient::isMaximizable() const
{
    if (!isResizable()) {
        return false;
    }
    if (rules()->checkMaximize(MaximizeRestore) != MaximizeRestore ||
            rules()->checkMaximize(MaximizeFull) != MaximizeFull) {
        return false;
    }
    return true;
}

bool XdgToplevelClient::isMinimizable() const
{
    if (isSpecialWindow() && !isTransient()) {
        return false;
    }
    if (!rules()->checkMinimize(true)) {
        return false;
    }
    return true;
}

bool XdgToplevelClient::isTransient() const
{
    return m_isTransient;
}

bool XdgToplevelClient::userCanSetFullScreen() const
{
    return true;
}

bool XdgToplevelClient::userCanSetNoBorder() const
{
    if (m_serverDecoration) {
        switch (m_serverDecoration->mode()) {
        case ServerSideDecorationManagerInterface::Mode::Server:
            return !isFullScreen() && !isShade();
        case ServerSideDecorationManagerInterface::Mode::Client:
        case ServerSideDecorationManagerInterface::Mode::None:
            return false;
        }
    }
    if (m_xdgDecoration) {
        switch (m_xdgDecoration->preferredMode()) {
        case XdgToplevelDecorationV1Interface::Mode::Server:
        case XdgToplevelDecorationV1Interface::Mode::Undefined:
            return !isFullScreen() && !isShade();
        case XdgToplevelDecorationV1Interface::Mode::Client:
            return false;
        }
    }
    return false;
}

bool XdgToplevelClient::noBorder() const
{
    if (m_serverDecoration) {
        switch (m_serverDecoration->mode()) {
        case ServerSideDecorationManagerInterface::Mode::Server:
            return m_userNoBorder || isFullScreen();
        case ServerSideDecorationManagerInterface::Mode::Client:
        case ServerSideDecorationManagerInterface::Mode::None:
            return true;
        }
    }
    if (m_xdgDecoration) {
        switch (m_xdgDecoration->preferredMode()) {
        case XdgToplevelDecorationV1Interface::Mode::Server:
        case XdgToplevelDecorationV1Interface::Mode::Undefined:
            return m_userNoBorder || isFullScreen();
        case XdgToplevelDecorationV1Interface::Mode::Client:
            return true;
        }
    }
    return true;
}

void XdgToplevelClient::setNoBorder(bool set)
{
    if (!userCanSetNoBorder()) {
        return;
    }
    set = rules()->checkNoBorder(set);
    if (m_userNoBorder == set) {
        return;
    }
    m_userNoBorder = set;
    updateDecoration(true, false);
    updateWindowRules(Rules::NoBorder);
}

void XdgToplevelClient::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force && ((!isDecorated() && noBorder()) || (isDecorated() && !noBorder()))) {
        return;
    }
    const QRect oldFrameGeometry = frameGeometry();
    const QRect oldClientGeometry = clientGeometry();
    blockGeometryUpdates(true);
    if (force) {
        destroyDecoration();
    }
    if (!noBorder()) {
        createDecoration(oldFrameGeometry);
    } else {
        destroyDecoration();
    }
    if (m_serverDecoration && isDecorated()) {
        m_serverDecoration->setMode(ServerSideDecorationManagerInterface::Mode::Server);
    }
    if (m_xdgDecoration) {
        if (isDecorated() || m_userNoBorder) {
            m_xdgDecoration->sendConfigure(XdgToplevelDecorationV1Interface::Mode::Server);
        } else {
            m_xdgDecoration->sendConfigure(XdgToplevelDecorationV1Interface::Mode::Client);
        }
        scheduleConfigure();
    }
    updateShadow();
    if (check_workspace_pos) {
        checkWorkspacePosition(oldFrameGeometry, -2, oldClientGeometry);
    }
    blockGeometryUpdates(false);
}

bool XdgToplevelClient::supportsWindowRules() const
{
    return !m_plasmaShellSurface;
}

StrutRect XdgToplevelClient::strutRect(StrutArea area) const
{
    if (!hasStrut()) {
        return StrutRect();
    }

    const QRect windowRect = frameGeometry();
    const QRect outputRect = screens()->geometry(screen());

    const bool left = windowRect.left() == outputRect.left();
    const bool right = windowRect.right() == outputRect.right();
    const bool top = windowRect.top() == outputRect.top();
    const bool bottom = windowRect.bottom() == outputRect.bottom();
    const bool horizontal = width() >= height();

    switch (area) {
    case StrutAreaTop:
        if (top && ((!left && !right) || horizontal)) {
            return StrutRect(windowRect, StrutAreaTop);
        }
        return StrutRect();
    case StrutAreaRight:
        if (right && ((!top && !bottom) || !horizontal)) {
            return StrutRect(windowRect, StrutAreaRight);
        }
        return StrutRect();
    case StrutAreaBottom:
        if (bottom && ((!left && !right) || horizontal)) {
            return StrutRect(windowRect, StrutAreaBottom);
        }
        return StrutRect();
    case StrutAreaLeft:
        if (left && ((!top && !bottom) || !horizontal)) {
            return StrutRect(windowRect, StrutAreaLeft);
        }
        return StrutRect();
    default:
        return StrutRect();
    }
}

bool XdgToplevelClient::hasStrut() const
{
    if (!isShown(true)) {
        return false;
    }
    if (!m_plasmaShellSurface) {
        return false;
    }
    if (m_plasmaShellSurface->role() != PlasmaShellSurfaceInterface::Role::Panel) {
        return false;
    }
    return m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible;
}

void XdgToplevelClient::showOnScreenEdge()
{
    if (!m_plasmaShellSurface) {
        return;
    }
    
    // ShowOnScreenEdge can be called by an Edge, and hideClient could destroy the Edge
    // Use the singleshot to avoid use-after-free
    QTimer::singleShot(0, this, [this](){
        hideClient(false);
        workspace()->raiseClient(this);
        if (m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide) {
            m_plasmaShellSurface->showAutoHidingPanel();
        } 
    });
}

void XdgToplevelClient::closeWindow()
{
    if (isCloseable()) {
        sendPing(PingReason::CloseWindow);
        m_shellSurface->sendClose();
    }
}

XdgSurfaceConfigure *XdgToplevelClient::sendRoleConfigure() const
{
    const quint32 serial = m_shellSurface->sendConfigure(requestedClientSize(), m_requestedStates);

    XdgToplevelConfigure *configureEvent = new XdgToplevelConfigure();
    configureEvent->position = requestedPos();
    configureEvent->size = requestedSize();
    configureEvent->states = m_requestedStates;
    configureEvent->serial = serial;

    return configureEvent;
}

bool XdgToplevelClient::stateCompare() const
{
    if (m_requestedStates != m_acknowledgedStates) {
        return true;
    }
    return XdgSurfaceClient::stateCompare();
}

void XdgToplevelClient::handleRoleCommit()
{
    auto configureEvent = static_cast<XdgToplevelConfigure *>(lastAcknowledgedConfigure());
    if (configureEvent) {
        handleStatesAcknowledged(configureEvent->states);
    }
}

void XdgToplevelClient::doMinimize()
{
    if (isMinimized()) {
        workspace()->clientHidden(this);
    } else {
        emit windowShown(this);
    }
    workspace()->updateMinimizedOfTransients(this);
}

void XdgToplevelClient::doResizeSync()
{
    requestGeometry(moveResizeGeometry());
}

void XdgToplevelClient::doSetActive()
{
    WaylandClient::doSetActive();

    if (isActive()) {
        m_requestedStates |= XdgToplevelInterface::State::Activated;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::Activated;
    }

    scheduleConfigure();
}

void XdgToplevelClient::doSetFullScreen()
{
    if (isFullScreen()) {
        m_requestedStates |= XdgToplevelInterface::State::FullScreen;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::FullScreen;
    }

    scheduleConfigure();
}

void XdgToplevelClient::doSetMaximized()
{
    if (requestedMaximizeMode() & MaximizeHorizontal) {
        m_requestedStates |= XdgToplevelInterface::State::MaximizedHorizontal;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::MaximizedHorizontal;
    }

    if (requestedMaximizeMode() & MaximizeVertical) {
        m_requestedStates |= XdgToplevelInterface::State::MaximizedVertical;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::MaximizedVertical;
    }

    scheduleConfigure();
}

static Qt::Edges anchorsForQuickTileMode(QuickTileMode mode)
{
    if (mode == QuickTileMode(QuickTileFlag::None)) {
        return Qt::Edges();
    }

    Qt::Edges anchors = Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge;

    if ((mode & QuickTileFlag::Left) && !(mode & QuickTileFlag::Right)) {
        anchors &= ~Qt::RightEdge;
    }
    if ((mode & QuickTileFlag::Right) && !(mode & QuickTileFlag::Left)) {
        anchors &= ~Qt::LeftEdge;
    }

    if ((mode & QuickTileFlag::Top) && !(mode & QuickTileFlag::Bottom)) {
        anchors &= ~Qt::BottomEdge;
    }
    if ((mode & QuickTileFlag::Bottom) && !(mode & QuickTileFlag::Top)) {
        anchors &= ~Qt::TopEdge;
    }

    return anchors;
}

void XdgToplevelClient::doSetQuickTileMode()
{
    const Qt::Edges anchors = anchorsForQuickTileMode(quickTileMode());

    if (anchors & Qt::LeftEdge) {
        m_requestedStates |= XdgToplevelInterface::State::TiledLeft;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::TiledLeft;
    }

    if (anchors & Qt::RightEdge) {
        m_requestedStates |= XdgToplevelInterface::State::TiledRight;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::TiledRight;
    }

    if (anchors & Qt::TopEdge) {
        m_requestedStates |= XdgToplevelInterface::State::TiledTop;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::TiledTop;
    }

    if (anchors & Qt::BottomEdge) {
        m_requestedStates |= XdgToplevelInterface::State::TiledBottom;
    } else {
        m_requestedStates &= ~XdgToplevelInterface::State::TiledBottom;
    }

    scheduleConfigure();
}

bool XdgToplevelClient::doStartMoveResize()
{
    if (moveResizePointerMode() != PositionCenter) {
        m_requestedStates |= XdgToplevelInterface::State::Resizing;
    }

    scheduleConfigure();
    return true;
}

void XdgToplevelClient::doFinishMoveResize()
{
    m_requestedStates &= ~XdgToplevelInterface::State::Resizing;
    scheduleConfigure();
}

bool XdgToplevelClient::takeFocus()
{
    if (wantsInput()) {
        sendPing(PingReason::FocusWindow);
        setActive(true);
    }
    if (!keepAbove() && !isOnScreenDisplay() && !belongsToDesktop()) {
        workspace()->setShowingDesktop(false);
    }
    return true;
}

bool XdgToplevelClient::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus());
}

bool XdgToplevelClient::dockWantsInput() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Panel) {
            return m_plasmaShellSurface->panelTakesFocus();
        }
    }
    return false;
}

bool XdgToplevelClient::acceptsFocus() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::OnScreenDisplay ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::ToolTip) {
            return false;
        }
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Notification ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::CriticalNotification) {
            return m_plasmaShellSurface->panelTakesFocus();
        }
    }
    return !isZombie() && readyForPainting();
}

Layer XdgToplevelClient::layerForDock() const
{
    if (m_plasmaShellSurface) {
        switch (m_plasmaShellSurface->panelBehavior()) {
        case PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover:
            return NormalLayer;
        case PlasmaShellSurfaceInterface::PanelBehavior::AutoHide:
        case PlasmaShellSurfaceInterface::PanelBehavior::WindowsGoBelow:
            return AboveLayer;
        case PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible:
            return DockLayer;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    return AbstractClient::layerForDock();
}

void XdgToplevelClient::handleWindowTitleChanged()
{
    setCaption(m_shellSurface->windowTitle());
}

void XdgToplevelClient::handleWindowClassChanged()
{
    const QByteArray applicationId = m_shellSurface->windowClass().toUtf8();
    setResourceClass(resourceName(), applicationId);
    if (shellSurface()->isConfigured() && supportsWindowRules()) {
        evaluateWindowRules();
    }
    setDesktopFileName(applicationId);
}

void XdgToplevelClient::handleWindowMenuRequested(SeatInterface *seat, const QPoint &surfacePos,
                                                  quint32 serial)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    performMouseCommand(Options::MouseOperationsMenu, pos() + surfacePos);
}

void XdgToplevelClient::handleMoveRequested(SeatInterface *seat, quint32 serial)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    performMouseCommand(Options::MouseMove, Cursors::self()->mouse()->pos());
}

void XdgToplevelClient::handleResizeRequested(SeatInterface *seat, Qt::Edges edges, quint32 serial)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    if (!isResizable() || isShade()) {
        return;
    }
    if (isMoveResize()) {
        finishMoveResize(false);
    }
    setMoveResizePointerButtonDown(true);
    setMoveOffset(Cursors::self()->mouse()->pos() - pos());  // map from global
    setInvertedMoveOffset(rect().bottomRight() - moveOffset());
    setUnrestrictedMoveResize(false);
    auto toPosition = [edges] {
        Position position = PositionCenter;
        if (edges.testFlag(Qt::TopEdge)) {
            position = PositionTop;
        } else if (edges.testFlag(Qt::BottomEdge)) {
            position = PositionBottom;
        }
        if (edges.testFlag(Qt::LeftEdge)) {
            position = Position(position | PositionLeft);
        } else if (edges.testFlag(Qt::RightEdge)) {
            position = Position(position | PositionRight);
        }
        return position;
    };
    setMoveResizePointerMode(toPosition());
    if (!startMoveResize()) {
        setMoveResizePointerButtonDown(false);
    }
    updateCursor();
}

void XdgToplevelClient::handleStatesAcknowledged(const XdgToplevelInterface::States &states)
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

void XdgToplevelClient::handleMaximizeRequested()
{
    if (m_isInitialized) {
        maximize(MaximizeFull);
        scheduleConfigure();
    } else {
        m_initialStates |= XdgToplevelInterface::State::Maximized;
    }
}

void XdgToplevelClient::handleUnmaximizeRequested()
{
    if (m_isInitialized) {
        maximize(MaximizeRestore);
        scheduleConfigure();
    } else {
        m_initialStates &= ~XdgToplevelInterface::State::Maximized;
    }
}

void XdgToplevelClient::handleFullscreenRequested(OutputInterface *output)
{
    Q_UNUSED(output)
    if (m_isInitialized) {
        setFullScreen(/* set */ true, /* user */ false);
        scheduleConfigure();
    } else {
        m_initialStates |= XdgToplevelInterface::State::FullScreen;
    }
}

void XdgToplevelClient::handleUnfullscreenRequested()
{
    if (m_isInitialized) {
        setFullScreen(/* set */ false, /* user */ false);
        scheduleConfigure();
    } else {
        m_initialStates &= ~XdgToplevelInterface::State::FullScreen;
    }
}

void XdgToplevelClient::handleMinimizeRequested()
{
    performMouseCommand(Options::MouseMinimize, Cursors::self()->mouse()->pos());
}

void XdgToplevelClient::handleTransientForChanged()
{
    SurfaceInterface *transientForSurface = nullptr;
    if (XdgToplevelInterface *parentToplevel = m_shellSurface->parentXdgToplevel()) {
        transientForSurface = parentToplevel->surface();
    }
    if (!transientForSurface) {
        transientForSurface = waylandServer()->findForeignTransientForSurface(surface());
    }
    AbstractClient *transientForClient = waylandServer()->findClient(transientForSurface);
    if (transientForClient != transientFor()) {
        if (transientFor()) {
            transientFor()->removeTransient(this);
        }
        if (transientForClient) {
            transientForClient->addTransient(this);
        }
        setTransientFor(transientForClient);
    }
    m_isTransient = transientForClient;
}

void XdgToplevelClient::handleForeignTransientForChanged(SurfaceInterface *child)
{
    if (surface() == child) {
        handleTransientForChanged();
    }
}

void XdgToplevelClient::handlePingTimeout(quint32 serial)
{
    auto pingIt = m_pings.find(serial);
    if (pingIt == m_pings.end()) {
        return;
    }
    if (pingIt.value() == PingReason::CloseWindow) {
        qCDebug(KWIN_CORE) << "Final ping timeout on a close attempt, asking to kill:" << caption();

        //for internal windows, killing the window will delete this
        QPointer<QObject> guard(this);
        killWindow();
        if (!guard) {
            return;
        }
    }
    m_pings.erase(pingIt);
}

void XdgToplevelClient::handlePingDelayed(quint32 serial)
{
    auto it = m_pings.find(serial);
    if (it != m_pings.end()) {
        qCDebug(KWIN_CORE) << "First ping timeout:" << caption();
        setUnresponsive(true);
    }
}

void XdgToplevelClient::handlePongReceived(quint32 serial)
{
    auto it = m_pings.find(serial);
    if (it != m_pings.end()) {
        setUnresponsive(false);
        m_pings.erase(it);
    }
}

void XdgToplevelClient::sendPing(PingReason reason)
{
    XdgShellInterface *shell = m_shellSurface->shell();
    XdgSurfaceInterface *surface = m_shellSurface->xdgSurface();

    const quint32 serial = shell->ping(surface);
    m_pings.insert(serial, reason);
}

MaximizeMode XdgToplevelClient::initialMaximizeMode() const
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

bool XdgToplevelClient::initialFullScreenMode() const
{
    return m_initialStates & XdgToplevelInterface::State::FullScreen;
}

void XdgToplevelClient::initialize()
{
    blockGeometryUpdates(true);

    bool needsPlacement = !isInitialPositionSet();

    updateDecoration(false, false);

    if (supportsWindowRules()) {
        setupWindowRules(false);

        const QRect originalGeometry = frameGeometry();
        const QRect ruledGeometry = rules()->checkGeometry(originalGeometry, true);
        if (originalGeometry != ruledGeometry) {
            setFrameGeometry(ruledGeometry);
        }
        maximize(rules()->checkMaximize(initialMaximizeMode(), true));
        setFullScreen(rules()->checkFullScreen(initialFullScreenMode(), true), false);
        setDesktop(rules()->checkDesktop(desktop(), true));
        setDesktopFileName(rules()->checkDesktopFile(desktopFileName(), true).toUtf8());
        if (rules()->checkMinimize(isMinimized(), true)) {
            minimize(true); // No animation.
        }
        setSkipTaskbar(rules()->checkSkipTaskbar(skipTaskbar(), true));
        setSkipPager(rules()->checkSkipPager(skipPager(), true));
        setSkipSwitcher(rules()->checkSkipSwitcher(skipSwitcher(), true));
        setKeepAbove(rules()->checkKeepAbove(keepAbove(), true));
        setKeepBelow(rules()->checkKeepBelow(keepBelow(), true));
        setShortcut(rules()->checkShortcut(shortcut().toString(), true));

        // Don't place the client if its position is set by a rule.
        if (rules()->checkPosition(invalidPoint, true) != invalidPoint) {
            needsPlacement = false;
        }

        // Don't place the client if the maximize state is set by a rule.
        if (requestedMaximizeMode() != MaximizeRestore) {
            needsPlacement = false;
        }

        discardTemporaryRules();
        RuleBook::self()->discardUsed(this, false); // Remove Apply Now rules.
        updateWindowRules(Rules::All);
    }
    if (isFullScreen()) {
        needsPlacement = false;
    }
    if (needsPlacement) {
        const QRect area = workspace()->clientArea(PlacementArea, Screens::self()->current(), desktop());
        placeIn(area);
    }

    blockGeometryUpdates(false);
    scheduleConfigure();
    updateColorScheme();
    m_isInitialized = true;
}

void XdgToplevelClient::updateMaximizeMode(MaximizeMode maximizeMode)
{
    if (m_maximizeMode == maximizeMode) {
        return;
    }
    m_maximizeMode = maximizeMode;
    updateWindowRules(Rules::MaximizeVert | Rules::MaximizeHoriz);
    emit clientMaximizedStateChanged(this, maximizeMode);
    emit clientMaximizedStateChanged(this, maximizeMode & MaximizeHorizontal, maximizeMode & MaximizeVertical);
}

void XdgToplevelClient::updateFullScreenMode(bool set)
{
    if (m_isFullScreen == set) {
        return;
    }
    m_isFullScreen = set;
    updateWindowRules(Rules::Fullscreen);
    emit fullScreenChanged();
}

QString XdgToplevelClient::preferredColorScheme() const
{
    if (m_paletteInterface) {
        return rules()->checkDecoColor(m_paletteInterface->palette());
    }
    return rules()->checkDecoColor(QString());
}

void XdgToplevelClient::installAppMenu(AppMenuInterface *appMenu)
{
    m_appMenuInterface = appMenu;

    auto updateMenu = [this](const AppMenuInterface::InterfaceAddress &address) {
        updateApplicationMenuServiceName(address.serviceName);
        updateApplicationMenuObjectPath(address.objectPath);
    };
    connect(m_appMenuInterface, &AppMenuInterface::addressChanged, this, updateMenu);
    updateMenu(appMenu->address());
}

void XdgToplevelClient::installServerDecoration(ServerSideDecorationInterface *decoration)
{
    m_serverDecoration = decoration;

    connect(m_serverDecoration, &ServerSideDecorationInterface::destroyed, this, [this] {
        if (!isZombie() && readyForPainting()) {
            updateDecoration(/* check_workspace_pos */ true);
        }
    });
    connect(m_serverDecoration, &ServerSideDecorationInterface::modeRequested, this,
        [this] (ServerSideDecorationManagerInterface::Mode mode) {
            const bool changed = mode != m_serverDecoration->mode();
            if (changed && readyForPainting()) {
                updateDecoration(/* check_workspace_pos */ false);
            }
        }
    );
    if (readyForPainting()) {
        updateDecoration(/* check_workspace_pos */ true);
    }
}

void XdgToplevelClient::installXdgDecoration(XdgToplevelDecorationV1Interface *decoration)
{
    m_xdgDecoration = decoration;

    connect(m_xdgDecoration, &XdgToplevelDecorationV1Interface::destroyed, this, [this] {
        if (!isZombie() && m_isInitialized) {
            updateDecoration(/* check_workspace_pos */ true);
        }
    });
    connect(m_xdgDecoration, &XdgToplevelDecorationV1Interface::preferredModeChanged, this, [this] {
        if (m_isInitialized) {
            // force is true as we must send a new configure response.
            updateDecoration(/* check_workspace_pos */ false, /* force */ true);
        }
    });
}

void XdgToplevelClient::installPalette(ServerSideDecorationPaletteInterface *palette)
{
    m_paletteInterface = palette;

    connect(m_paletteInterface, &ServerSideDecorationPaletteInterface::paletteChanged,
            this, &XdgToplevelClient::updateColorScheme);
    connect(m_paletteInterface, &QObject::destroyed,
            this, &XdgToplevelClient::updateColorScheme);
    updateColorScheme();
}

/**
 * \todo This whole plasma shell surface thing doesn't seem right. It turns xdg-toplevel into
 * something completely different! Perhaps plasmashell surfaces need to be implemented via a
 * proprietary protocol that doesn't piggyback on existing shell surface protocols. It'll lead
 * to cleaner code and will be technically correct, but I'm not sure whether this is do-able.
 */
void XdgToplevelClient::installPlasmaShellSurface(PlasmaShellSurfaceInterface *shellSurface)
{
    m_plasmaShellSurface = shellSurface;

    auto updatePosition = [this, shellSurface] { move(shellSurface->position()); };
    auto updateRole = [this, shellSurface] {
        NET::WindowType type = NET::Unknown;
        switch (shellSurface->role()) {
        case PlasmaShellSurfaceInterface::Role::Desktop:
            type = NET::Desktop;
            break;
        case PlasmaShellSurfaceInterface::Role::Panel:
            type = NET::Dock;
            break;
        case PlasmaShellSurfaceInterface::Role::OnScreenDisplay:
            type = NET::OnScreenDisplay;
            break;
        case PlasmaShellSurfaceInterface::Role::Notification:
            type = NET::Notification;
            break;
        case PlasmaShellSurfaceInterface::Role::ToolTip:
            type = NET::Tooltip;
            break;
        case PlasmaShellSurfaceInterface::Role::CriticalNotification:
            type = NET::CriticalNotification;
            break;
        case PlasmaShellSurfaceInterface::Role::Normal:
        default:
            type = NET::Normal;
            break;
        }
        if (m_windowType == type) {
            return;
        }
        m_windowType = type;
        switch (m_windowType) {
        case NET::Desktop:
        case NET::Dock:
        case NET::OnScreenDisplay:
        case NET::Notification:
        case NET::CriticalNotification:
        case NET::Tooltip:
            setOnAllDesktops(true);
            break;
        default:
            break;
        }
        workspace()->updateClientArea();
    };
    connect(shellSurface, &PlasmaShellSurfaceInterface::positionChanged, this, updatePosition);
    connect(shellSurface, &PlasmaShellSurfaceInterface::roleChanged, this, updateRole);
    connect(shellSurface, &PlasmaShellSurfaceInterface::panelBehaviorChanged, this, [this] {
        updateShowOnScreenEdge();
        workspace()->updateClientArea();
    });
    connect(shellSurface, &PlasmaShellSurfaceInterface::panelAutoHideHideRequested, this, [this] {
        if (m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide) {
            hideClient(true);
            m_plasmaShellSurface->hideAutoHidingPanel();
        }
        updateShowOnScreenEdge();
    });
    connect(shellSurface, &PlasmaShellSurfaceInterface::panelAutoHideShowRequested, this, [this] {
        hideClient(false);
        ScreenEdges::self()->reserve(this, ElectricNone);
        m_plasmaShellSurface->showAutoHidingPanel();
    });
    connect(shellSurface, &PlasmaShellSurfaceInterface::panelTakesFocusChanged, this, [this] {
        if (m_plasmaShellSurface->panelTakesFocus()) {
            workspace()->activateClient(this);
        }
    });
    if (shellSurface->isPositionSet()) {
        updatePosition();
    }
    updateRole();
    updateShowOnScreenEdge();
    connect(this, &XdgToplevelClient::frameGeometryChanged,
            this, &XdgToplevelClient::updateShowOnScreenEdge);
    connect(this, &XdgToplevelClient::windowShown,
            this, &XdgToplevelClient::updateShowOnScreenEdge);

    setSkipTaskbar(shellSurface->skipTaskbar());
    connect(shellSurface, &PlasmaShellSurfaceInterface::skipTaskbarChanged, this, [this] {
        setSkipTaskbar(m_plasmaShellSurface->skipTaskbar());
    });

    setSkipSwitcher(shellSurface->skipSwitcher());
    connect(shellSurface, &PlasmaShellSurfaceInterface::skipSwitcherChanged, this, [this] {
        setSkipSwitcher(m_plasmaShellSurface->skipSwitcher());
    });
}

void XdgToplevelClient::updateShowOnScreenEdge()
{
    if (!ScreenEdges::self()) {
        return;
    }
    if (!readyForPainting() || !m_plasmaShellSurface ||
            m_plasmaShellSurface->role() != PlasmaShellSurfaceInterface::Role::Panel) {
        ScreenEdges::self()->reserve(this, ElectricNone);
        return;
    }
    const PlasmaShellSurfaceInterface::PanelBehavior panelBehavior = m_plasmaShellSurface->panelBehavior();
    if ((panelBehavior == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide && isHidden()) ||
            panelBehavior == PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover) {
        // Screen edge API requires an edge, thus we need to figure out which edge the window borders.
        const QRect clientGeometry = frameGeometry();
        Qt::Edges edges;
        for (int i = 0; i < screens()->count(); i++) {
            const QRect screenGeometry = screens()->geometry(i);
            if (screenGeometry.left() == clientGeometry.left()) {
                edges |= Qt::LeftEdge;
            }
            if (screenGeometry.right() == clientGeometry.right()) {
                edges |= Qt::RightEdge;
            }
            if (screenGeometry.top() == clientGeometry.top()) {
                edges |= Qt::TopEdge;
            }
            if (screenGeometry.bottom() == clientGeometry.bottom()) {
                edges |= Qt::BottomEdge;
            }
        }

        // A panel might border multiple screen edges. E.g. a horizontal panel at the bottom will
        // also border the left and right edge. Let's remove such cases.
        if (edges & Qt::LeftEdge && edges & Qt::RightEdge) {
            edges = edges & (~(Qt::LeftEdge | Qt::RightEdge));
        }
        if (edges & Qt::TopEdge && edges & Qt::BottomEdge) {
            edges = edges & (~(Qt::TopEdge | Qt::BottomEdge));
        }

        // It's still possible that a panel borders two edges, e.g. bottom and left
        // in that case the one which is sharing more with the edge wins.
        auto check = [clientGeometry](Qt::Edges edges, Qt::Edge horizontal, Qt::Edge vertical) {
            if (edges & horizontal && edges & vertical) {
                if (clientGeometry.width() >= clientGeometry.height()) {
                    return edges & ~horizontal;
                } else {
                    return edges & ~vertical;
                }
            }
            return edges;
        };
        edges = check(edges, Qt::LeftEdge, Qt::TopEdge);
        edges = check(edges, Qt::LeftEdge, Qt::BottomEdge);
        edges = check(edges, Qt::RightEdge, Qt::TopEdge);
        edges = check(edges, Qt::RightEdge, Qt::BottomEdge);

        ElectricBorder border = ElectricNone;
        if (edges & Qt::LeftEdge) {
            border = ElectricLeft;
        }
        if (edges & Qt::RightEdge) {
            border = ElectricRight;
        }
        if (edges & Qt::TopEdge) {
            border = ElectricTop;
        }
        if (edges & Qt::BottomEdge) {
            border = ElectricBottom;
        }
        ScreenEdges::self()->reserve(this, border);
    } else {
        ScreenEdges::self()->reserve(this, ElectricNone);
    }
}

void XdgToplevelClient::updateClientArea()
{
    if (hasStrut()) {
        workspace()->updateClientArea();
    }
}

void XdgToplevelClient::setupWindowManagementIntegration()
{
    if (isLockScreen()) {
        return;
    }
    connect(surface(), &SurfaceInterface::mapped,
            this, &XdgToplevelClient::setupWindowManagementInterface);
}

void XdgToplevelClient::setupPlasmaShellIntegration()
{
    connect(surface(), &SurfaceInterface::mapped,
            this, &XdgToplevelClient::updateShowOnScreenEdge);
    connect(this, &XdgToplevelClient::frameGeometryChanged,
            this, &XdgToplevelClient::updateClientArea);
}

void XdgToplevelClient::setFullScreen(bool set, bool user)
{
    set = rules()->checkFullScreen(set);

    const bool wasFullscreen = isFullScreen();
    if (wasFullscreen == set) {
        return;
    }
    if (isSpecialWindow()) {
        return;
    }
    if (user && !userCanSetFullScreen()) {
        return;
    }

    if (wasFullscreen) {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event
    } else {
        m_fullScreenGeometryRestore = frameGeometry();
    }
    m_isFullScreen = set;

    if (set) {
        workspace()->raiseClient(this);
    }
    StackingUpdatesBlocker blocker1(workspace());
    GeometryUpdatesBlocker blocker2(this);
    if (set) {
        dontMoveResize();
    }

    workspace()->updateClientLayer(this);   // active fullscreens get different layer
    updateDecoration(false, false);

    if (set) {
        setFrameGeometry(workspace()->clientArea(FullScreenArea, this));
    } else {
        if (m_fullScreenGeometryRestore.isValid()) {
            int currentScreen = screen();
            setFrameGeometry(QRect(m_fullScreenGeometryRestore.topLeft(),
                                   constrainFrameSize(m_fullScreenGeometryRestore.size())));
            if( currentScreen != screen())
                workspace()->sendClientToScreen( this, currentScreen );
        } else {
            // this can happen when the window was first shown already fullscreen,
            // so let the client set the size by itself
            setFrameGeometry(QRect(workspace()->clientArea(PlacementArea, this).topLeft(), QSize(0, 0)));
        }
    }

    doSetFullScreen();

    updateWindowRules(Rules::Fullscreen|Rules::Position|Rules::Size);
    emit fullScreenChanged();
}

/**
 * \todo Move to AbstractClient.
 */
static bool changeMaximizeRecursion = false;
void XdgToplevelClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    if (changeMaximizeRecursion) {
        return;
    }

    if (!isResizable()) {
        return;
    }

    const QRect clientArea = isElectricBorderMaximizing() ?
        workspace()->clientArea(MaximizeArea, Cursors::self()->mouse()->pos(), desktop()) :
        workspace()->clientArea(MaximizeArea, this);

    const MaximizeMode oldMode = m_requestedMaximizeMode;
    const QRect oldGeometry = frameGeometry();

    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if (!adjust) {
        if (vertical)
            m_requestedMaximizeMode = MaximizeMode(m_requestedMaximizeMode ^ MaximizeVertical);
        if (horizontal)
            m_requestedMaximizeMode = MaximizeMode(m_requestedMaximizeMode ^ MaximizeHorizontal);
    }

    m_requestedMaximizeMode = rules()->checkMaximize(m_requestedMaximizeMode);
    if (!adjust && m_requestedMaximizeMode == oldMode) {
        return;
    }

    StackingUpdatesBlocker blocker(workspace());
    if (m_requestedMaximizeMode != MaximizeRestore) {
        dontMoveResize();
    }

    // call into decoration update borders
    if (isDecorated() && decoration()->client() && !(options->borderlessMaximizedWindows() && m_requestedMaximizeMode == KWin::MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = decoration()->client().toStrongRef();
        if ((m_requestedMaximizeMode & MaximizeVertical) != (oldMode & MaximizeVertical)) {
            emit c->maximizedVerticallyChanged(m_requestedMaximizeMode & MaximizeVertical);
        }
        if ((m_requestedMaximizeMode & MaximizeHorizontal) != (oldMode & MaximizeHorizontal)) {
            emit c->maximizedHorizontallyChanged(m_requestedMaximizeMode & MaximizeHorizontal);
        }
        if ((m_requestedMaximizeMode == MaximizeFull) != (oldMode == MaximizeFull)) {
            emit c->maximizedChanged(m_requestedMaximizeMode & MaximizeFull);
        }
        changeMaximizeRecursion = false;
    }

    if (options->borderlessMaximizedWindows()) {
        // triggers a maximize change.
        // The next setNoBorder interation will exit since there's no change but the first recursion pullutes the restore geometry
        changeMaximizeRecursion = true;
        setNoBorder(rules()->checkNoBorder(m_requestedMaximizeMode == MaximizeFull));
        changeMaximizeRecursion = false;
    }

    // Conditional quick tiling exit points
    const auto oldQuickTileMode = quickTileMode();
    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        if (oldMode == MaximizeFull &&
                !clientArea.contains(geometryRestore().center())) {
            // Not restoring on the same screen
            // TODO: The following doesn't work for some reason
            //quick_tile_mode = QuickTileNone; // And exit quick tile mode manually
        } else if ((oldMode == MaximizeVertical && m_requestedMaximizeMode == MaximizeRestore) ||
                  (oldMode == MaximizeFull && m_requestedMaximizeMode == MaximizeHorizontal)) {
            // Modifying geometry of a tiled window
            updateQuickTileMode(QuickTileFlag::None); // Exit quick tile mode without restoring geometry
        }
    }

    if (m_requestedMaximizeMode == MaximizeFull) {
        setGeometryRestore(oldGeometry);
        // TODO: Client has more checks
        if (options->electricBorderMaximize()) {
            updateQuickTileMode(QuickTileFlag::Maximize);
        } else {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            doSetQuickTileMode();
            emit quickTileModeChanged();
        }
        setFrameGeometry(workspace()->clientArea(MaximizeArea, this));
    } else {
        if (m_requestedMaximizeMode == MaximizeRestore) {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            doSetQuickTileMode();
            emit quickTileModeChanged();
        }

        if (geometryRestore().isValid()) {
            setFrameGeometry(geometryRestore());
        } else {
            setFrameGeometry(workspace()->clientArea(PlacementArea, this));
        }
    }

    doSetMaximized();
}

XdgPopupClient::XdgPopupClient(XdgPopupInterface *shellSurface)
    : XdgSurfaceClient(shellSurface->xdgSurface())
    , m_shellSurface(shellSurface)
{
    setDesktop(VirtualDesktopManager::self()->current());

    connect(shellSurface, &XdgPopupInterface::grabRequested,
            this, &XdgPopupClient::handleGrabRequested);
    connect(shellSurface, &XdgPopupInterface::initializeRequested,
            this, &XdgPopupClient::initialize);
    connect(shellSurface, &XdgPopupInterface::destroyed,
            this, &XdgPopupClient::destroyClient);
}

XdgPopupClient::~XdgPopupClient()
{
}

NET::WindowType XdgPopupClient::windowType(bool direct, int supported_types) const
{
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return NET::Normal;
}

bool XdgPopupClient::hasPopupGrab() const
{
    return m_haveExplicitGrab;
}

void XdgPopupClient::popupDone()
{
    m_shellSurface->sendPopupDone();
}

bool XdgPopupClient::isPopupWindow() const
{
    return true;
}

bool XdgPopupClient::isTransient() const
{
    return true;
}

bool XdgPopupClient::isResizable() const
{
    return false;
}

bool XdgPopupClient::isMovable() const
{
    return false;
}

bool XdgPopupClient::isMovableAcrossScreens() const
{
    return false;
}

bool XdgPopupClient::hasTransientPlacementHint() const
{
    return true;
}

static QPoint popupOffset(const QRect &anchorRect, const Qt::Edges anchorEdge,
                          const Qt::Edges gravity, const QSize popupSize)
{
    QPoint anchorPoint;
    switch (anchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        anchorPoint.setX(anchorRect.x());
        break;
    case Qt::RightEdge:
        anchorPoint.setX(anchorRect.x() + anchorRect.width());
        break;
    default:
        anchorPoint.setX(qRound(anchorRect.x() + anchorRect.width() / 2.0));
    }
    switch (anchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        anchorPoint.setY(anchorRect.y());
        break;
    case Qt::BottomEdge:
        anchorPoint.setY(anchorRect.y() + anchorRect.height());
        break;
    default:
        anchorPoint.setY(qRound(anchorRect.y() + anchorRect.height() / 2.0));
    }

    // calculate where the top left point of the popup will end up with the applied gravity
    // gravity indicates direction. i.e if gravitating towards the top the popup's bottom edge
    // will next to the anchor point
    QPoint popupPosAdjust;
    switch (gravity & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        popupPosAdjust.setX(-popupSize.width());
        break;
    case Qt::RightEdge:
        popupPosAdjust.setX(0);
        break;
    default:
        popupPosAdjust.setX(qRound(-popupSize.width() / 2.0));
    }
    switch (gravity & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        popupPosAdjust.setY(-popupSize.height());
        break;
    case Qt::BottomEdge:
        popupPosAdjust.setY(0);
        break;
    default:
        popupPosAdjust.setY(qRound(-popupSize.height() / 2.0));
    }

    return anchorPoint + popupPosAdjust;
}

QRect XdgPopupClient::transientPlacement(const QRect &bounds) const
{
    const XdgPositioner positioner = m_shellSurface->positioner();

    QSize desiredSize = size();
    if (desiredSize.isEmpty()) {
        desiredSize = positioner.size();
    }

    const QPoint parentPosition = transientFor()->framePosToClientPos(transientFor()->pos());

    // returns if a target is within the supplied bounds, optional edges argument states which side to check
    auto inBounds = [bounds](const QRect &target, Qt::Edges edges = Qt::LeftEdge | Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge) -> bool {
        if (edges & Qt::LeftEdge && target.left() < bounds.left()) {
            return false;
        }
        if (edges & Qt::TopEdge && target.top() < bounds.top()) {
            return false;
        }
        if (edges & Qt::RightEdge && target.right() > bounds.right()) {
            //normal QRect::right issue cancels out
            return false;
        }
        if (edges & Qt::BottomEdge && target.bottom() > bounds.bottom()) {
            return false;
        }
        return true;
    };

    QRect popupRect(popupOffset(positioner.anchorRect(), positioner.anchorEdges(), positioner.gravityEdges(), desiredSize) + positioner.offset() + parentPosition, desiredSize);

    //if that fits, we don't need to do anything
    if (inBounds(popupRect)) {
        return popupRect;
    }
    //otherwise apply constraint adjustment per axis in order XDG Shell Popup states

    if (positioner.flipConstraintAdjustments() & Qt::Horizontal) {
        if (!inBounds(popupRect, Qt::LeftEdge | Qt::RightEdge)) {
            //flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = positioner.anchorEdges();
            if (flippedAnchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedAnchorEdge ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedGravity = positioner.gravityEdges();
            if (flippedGravity & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedGravity ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedPopupRect = QRect(popupOffset(positioner.anchorRect(), flippedAnchorEdge, flippedGravity, desiredSize) + positioner.offset() + parentPosition, desiredSize);

            //if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupRect, Qt::LeftEdge | Qt::RightEdge)) {
                popupRect.moveLeft(flippedPopupRect.left());
            }
        }
    }
    if (positioner.slideConstraintAdjustments() & Qt::Horizontal) {
        if (!inBounds(popupRect, Qt::LeftEdge)) {
            popupRect.moveLeft(bounds.left());
        }
        if (!inBounds(popupRect, Qt::RightEdge)) {
            popupRect.moveRight(bounds.right());
        }
    }
    if (positioner.resizeConstraintAdjustments() & Qt::Horizontal) {
        QRect unconstrainedRect = popupRect;

        if (!inBounds(unconstrainedRect, Qt::LeftEdge)) {
            unconstrainedRect.setLeft(bounds.left());
        }
        if (!inBounds(unconstrainedRect, Qt::RightEdge)) {
            unconstrainedRect.setRight(bounds.right());
        }

        if (unconstrainedRect.isValid()) {
            popupRect = unconstrainedRect;
        }
    }

    if (positioner.flipConstraintAdjustments() & Qt::Vertical) {
        if (!inBounds(popupRect, Qt::TopEdge | Qt::BottomEdge)) {
            //flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = positioner.anchorEdges();
            if (flippedAnchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedAnchorEdge ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedGravity = positioner.gravityEdges();
            if (flippedGravity & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedGravity ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedPopupRect = QRect(popupOffset(positioner.anchorRect(), flippedAnchorEdge, flippedGravity, desiredSize) + positioner.offset() + parentPosition, desiredSize);

            //if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupRect, Qt::TopEdge | Qt::BottomEdge)) {
                popupRect.moveTop(flippedPopupRect.top());
            }
        }
    }
    if (positioner.slideConstraintAdjustments() & Qt::Vertical) {
        if (!inBounds(popupRect, Qt::TopEdge)) {
            popupRect.moveTop(bounds.top());
        }
        if (!inBounds(popupRect, Qt::BottomEdge)) {
            popupRect.moveBottom(bounds.bottom());
        }
    }
    if (positioner.resizeConstraintAdjustments() & Qt::Vertical) {
        QRect unconstrainedRect = popupRect;

        if (!inBounds(unconstrainedRect, Qt::TopEdge)) {
            unconstrainedRect.setTop(bounds.top());
        }
        if (!inBounds(unconstrainedRect, Qt::BottomEdge)) {
            unconstrainedRect.setBottom(bounds.bottom());
        }

        if (unconstrainedRect.isValid()) {
            popupRect = unconstrainedRect;
        }
    }

    return popupRect;
}

bool XdgPopupClient::isCloseable() const
{
    return false;
}

void XdgPopupClient::closeWindow()
{
}

bool XdgPopupClient::wantsInput() const
{
    return false;
}

bool XdgPopupClient::takeFocus()
{
    return false;
}

bool XdgPopupClient::acceptsFocus() const
{
    return false;
}

XdgSurfaceConfigure *XdgPopupClient::sendRoleConfigure() const
{
    const QPoint parentPosition = transientFor()->framePosToClientPos(transientFor()->pos());
    const QPoint popupPosition = requestedPos() - parentPosition;

    const quint32 serial = m_shellSurface->sendConfigure(QRect(popupPosition, requestedClientSize()));

    XdgSurfaceConfigure *configureEvent = new XdgSurfaceConfigure();
    configureEvent->position = requestedPos();
    configureEvent->size = requestedSize();
    configureEvent->serial = serial;

    return configureEvent;
}

void XdgPopupClient::handleGrabRequested(SeatInterface *seat, quint32 serial)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    m_haveExplicitGrab = true;
}

void XdgPopupClient::initialize()
{
    AbstractClient *parentClient = waylandServer()->findClient(m_shellSurface->parentSurface());
    parentClient->addTransient(this);
    setTransientFor(parentClient);

    blockGeometryUpdates(true);
    const QRect area = workspace()->clientArea(PlacementArea, Screens::self()->current(), desktop());
    placeIn(area);
    blockGeometryUpdates(false);

    scheduleConfigure();
}
void XdgPopupClient::installPlasmaShellSurface(PlasmaShellSurfaceInterface *shellSurface)
{
    m_plasmaShellSurface = shellSurface;

    auto updatePosition = [this, shellSurface] { move(shellSurface->position()); };
    connect(shellSurface, &PlasmaShellSurfaceInterface::positionChanged, this, updatePosition);
    if (shellSurface->isPositionSet()) {
        updatePosition();
    }
}

} // namespace KWin
