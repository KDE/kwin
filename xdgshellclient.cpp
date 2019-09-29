/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vladzzag@gmail.com>

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
#include "xdgshellclient.h"
#include "cursor.h"
#include "decorations/decoratedclient.h"
#include "decorations/decorationbridge.h"
#include "deleted.h"
#include "placement.h"
#include "screenedge.h"
#include "screens.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KWayland/Server/appmenu_interface.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/plasmashell_interface.h>
#include <KWayland/Server/plasmawindowmanagement_interface.h>
#include <KWayland/Server/qtsurfaceextension_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/server_decoration_interface.h>
#include <KWayland/Server/server_decoration_palette_interface.h>
#include <KWayland/Server/shadow_interface.h>
#include <KWayland/Server/subcompositor_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/xdgdecoration_interface.h>

#include <QFileInfo>

#include <sys/types.h>
#include <unistd.h>

#include <csignal>

Q_DECLARE_METATYPE(NET::WindowType)

using namespace KWayland::Server;

namespace KWin
{

XdgShellClient::XdgShellClient(XdgShellSurfaceInterface *surface)
    : AbstractClient()
    , m_xdgShellSurface(surface)
    , m_xdgShellPopup(nullptr)
{
    setSurface(surface->surface());
    init();
}

XdgShellClient::XdgShellClient(XdgShellPopupInterface *surface)
    : AbstractClient()
    , m_xdgShellSurface(nullptr)
    , m_xdgShellPopup(surface)
{
    setSurface(surface->surface());
    init();
}

XdgShellClient::~XdgShellClient() = default;

void XdgShellClient::init()
{
    m_requestGeometryBlockCounter++;

    connect(this, &XdgShellClient::desktopFileNameChanged, this, &XdgShellClient::updateIcon);
    createWindowId();
    setupCompositing();
    updateIcon();

    // TODO: Initialize with null rect.
    geom = QRect(0, 0, -1, -1);
    m_windowGeometry = QRect(0, 0, -1, -1);

    if (waylandServer()->inputMethodConnection() == surface()->client()) {
        m_windowType = NET::OnScreenDisplay;
    }

    connect(surface(), &SurfaceInterface::unmapped, this, &XdgShellClient::unmap);
    connect(surface(), &SurfaceInterface::unbound, this, &XdgShellClient::destroyClient);
    connect(surface(), &SurfaceInterface::destroyed, this, &XdgShellClient::destroyClient);

    if (m_xdgShellSurface) {
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::destroyed, this, &XdgShellClient::destroyClient);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::configureAcknowledged, this, &XdgShellClient::handleConfigureAcknowledged);

        m_caption = m_xdgShellSurface->title().simplified();
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::titleChanged, this, &XdgShellClient::handleWindowTitleChanged);
        QTimer::singleShot(0, this, &XdgShellClient::updateCaption);

        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::moveRequested, this, &XdgShellClient::handleMoveRequested);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::resizeRequested, this, &XdgShellClient::handleResizeRequested);

        // Determine the resource name, this is inspired from ICCCM 4.1.2.5
        // the binary name of the invoked client.
        QFileInfo info{m_xdgShellSurface->client()->executablePath()};
        QByteArray resourceName;
        if (info.exists()) {
            resourceName = info.fileName().toUtf8();
        }
        setResourceClass(resourceName, m_xdgShellSurface->windowClass());
        setDesktopFileName(m_xdgShellSurface->windowClass());
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::windowClassChanged, this, &XdgShellClient::handleWindowClassChanged);

        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::minimizeRequested, this, &XdgShellClient::handleMinimizeRequested);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::maximizedChanged, this, &XdgShellClient::handleMaximizeRequested);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::fullscreenChanged, this, &XdgShellClient::handleFullScreenRequested);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::windowMenuRequested, this, &XdgShellClient::handleWindowMenuRequested);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::transientForChanged, this, &XdgShellClient::handleTransientForChanged);
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::windowGeometryChanged, this, &XdgShellClient::handleWindowGeometryChanged);

        auto global = static_cast<XdgShellInterface *>(m_xdgShellSurface->global());
        connect(global, &XdgShellInterface::pingDelayed, this, &XdgShellClient::handlePingDelayed);
        connect(global, &XdgShellInterface::pingTimeout, this, &XdgShellClient::handlePingTimeout);
        connect(global, &XdgShellInterface::pongReceived, this, &XdgShellClient::handlePongReceived);

        auto configure = [this] {
            if (m_closing) {
                return;
            }
            if (m_requestGeometryBlockCounter != 0 || areGeometryUpdatesBlocked()) {
                return;
            }
            m_xdgShellSurface->configure(xdgSurfaceStates(), m_requestedClientSize);
        };
        connect(this, &AbstractClient::activeChanged, this, configure);
        connect(this, &AbstractClient::clientStartUserMovedResized, this, configure);
        connect(this, &AbstractClient::clientFinishUserMovedResized, this, configure);

        connect(this, &XdgShellClient::geometryChanged, this, &XdgShellClient::updateClientOutputs);
        connect(screens(), &Screens::changed, this, &XdgShellClient::updateClientOutputs);
    } else if (m_xdgShellPopup) {
        connect(m_xdgShellPopup, &XdgShellPopupInterface::configureAcknowledged, this, &XdgShellClient::handleConfigureAcknowledged);
        connect(m_xdgShellPopup, &XdgShellPopupInterface::grabRequested, this, &XdgShellClient::handleGrabRequested);
        connect(m_xdgShellPopup, &XdgShellPopupInterface::destroyed, this, &XdgShellClient::destroyClient);
        connect(m_xdgShellPopup, &XdgShellPopupInterface::windowGeometryChanged, this, &XdgShellClient::handleWindowGeometryChanged);
    }

    // set initial desktop
    setDesktop(VirtualDesktopManager::self()->current());

    // setup shadow integration
    updateShadow();
    connect(surface(), &SurfaceInterface::shadowChanged, this, &Toplevel::updateShadow);

    connect(waylandServer(), &WaylandServer::foreignTransientChanged, this, [this](KWayland::Server::SurfaceInterface *child) {
        if (child == surface()) {
            handleTransientForChanged();
        }
    });
    handleTransientForChanged();

    AbstractClient::updateColorScheme(QString());

    connect(surface(), &SurfaceInterface::committed, this, &XdgShellClient::finishInit);
}

void XdgShellClient::finishInit()
{
    disconnect(surface(), &SurfaceInterface::committed, this, &XdgShellClient::finishInit);

    connect(surface(), &SurfaceInterface::committed, this, &XdgShellClient::handleCommitted);

    bool needsPlacement = !isInitialPositionSet();

    if (supportsWindowRules()) {
        setupWindowRules(false);

        const QRect originalGeometry = QRect(pos(), sizeForClientSize(clientSize()));
        const QRect ruledGeometry = rules()->checkGeometry(originalGeometry, true);
        if (originalGeometry != ruledGeometry) {
            setFrameGeometry(ruledGeometry);
        }

        maximize(rules()->checkMaximize(maximizeMode(), true));

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
        updateColorScheme();

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

    m_requestGeometryBlockCounter--;
    if (m_requestGeometryBlockCounter == 0) {
        requestGeometry(m_blockedRequestGeometry);
    }

    m_isInitialized = true;
}

void XdgShellClient::destroyClient()
{
    m_closing = true;
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    if (tabBox->isDisplayed() && tabBox->currentClient() == this) {
        tabBox->nextPrev(true);
    }
#endif
    if (isMoveResize()) {
        leaveMoveResize();
    }

    // Replace ShellClient with an instance of Deleted in the stacking order.
    Deleted *deleted = Deleted::create(this);
    emit windowClosed(this, deleted);

    // Remove Force Temporarily rules.
    RuleBook::self()->discardUsed(this, true);

    destroyWindowManagementInterface();
    destroyDecoration();

    StackingUpdatesBlocker blocker(workspace());
    if (transientFor()) {
        transientFor()->removeTransient(this);
    }
    for (auto it = transients().constBegin(); it != transients().constEnd();) {
        if ((*it)->transientFor() == this) {
            removeTransient(*it);
            it = transients().constBegin(); // restart, just in case something more has changed with the list
        } else {
            ++it;
        }
    }

    waylandServer()->removeClient(this);

    deleted->unrefWindow();

    m_xdgShellSurface = nullptr;
    m_xdgShellPopup = nullptr;
    deleteClient(this);
}

void XdgShellClient::deleteClient(XdgShellClient *c)
{
    delete c;
}

QRect XdgShellClient::bufferGeometry() const
{
    return m_bufferGeometry;
}

QStringList XdgShellClient::activities() const
{
    // TODO: implement
    return QStringList();
}

QPoint XdgShellClient::clientContentPos() const
{
    return -1 * clientPos();
}

QSize XdgShellClient::clientSize() const
{
    return m_windowGeometry.size();
}

void XdgShellClient::debug(QDebug &stream) const
{
    stream.nospace();
    stream << "\'XdgShellClient:" << surface() << ";WMCLASS:" << resourceClass() << ":"
           << resourceName() << ";Caption:" << caption() << "\'";
}

bool XdgShellClient::belongsToDesktop() const
{
    const auto clients = waylandServer()->clients();

    return std::any_of(clients.constBegin(), clients.constEnd(),
        [this](const XdgShellClient *client) {
            if (belongsToSameApplication(client, SameApplicationChecks())) {
                return client->isDesktop();
            }
            return false;
        }
    );
}

Layer XdgShellClient::layerForDock() const
{
    if (m_plasmaShellSurface) {
        switch (m_plasmaShellSurface->panelBehavior()) {
        case PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover:
            return NormalLayer;
        case PlasmaShellSurfaceInterface::PanelBehavior::AutoHide:
            return AboveLayer;
        case PlasmaShellSurfaceInterface::PanelBehavior::WindowsGoBelow:
        case PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible:
            return DockLayer;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    return AbstractClient::layerForDock();
}

QRect XdgShellClient::transparentRect() const
{
    // TODO: implement
    return QRect();
}

NET::WindowType XdgShellClient::windowType(bool direct, int supported_types) const
{
    // TODO: implement
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

double XdgShellClient::opacity() const
{
    return m_opacity;
}

void XdgShellClient::setOpacity(double opacity)
{
    const qreal newOpacity = qBound(0.0, opacity, 1.0);
    if (newOpacity == m_opacity) {
        return;
    }
    const qreal oldOpacity = m_opacity;
    m_opacity = newOpacity;
    addRepaintFull();
    emit opacityChanged(this, oldOpacity);
}

void XdgShellClient::addDamage(const QRegion &damage)
{
    const int offsetX = m_bufferGeometry.x() - frameGeometry().x();
    const int offsetY = m_bufferGeometry.y() - frameGeometry().y();
    repaints_region += damage.translated(offsetX, offsetY);

    Toplevel::addDamage(damage);
}

void XdgShellClient::markAsMapped()
{
    if (!m_unmapped) {
        return;
    }

    m_unmapped = false;
    if (!ready_for_painting) {
        setReadyForPainting();
    } else {
        addRepaintFull();
        emit windowShown(this);
    }
    if (shouldExposeToWindowManagement()) {
        setupWindowManagementInterface();
    }
    updateShowOnScreenEdge();
}

void XdgShellClient::createDecoration(const QRect &oldGeom)
{
    KDecoration2::Decoration *decoration = Decoration::DecorationBridge::self()->createDecoration(this);
    if (decoration) {
        QMetaObject::invokeMethod(decoration, "update", Qt::QueuedConnection);
        connect(decoration, &KDecoration2::Decoration::shadowChanged, this, &Toplevel::updateShadow);
        connect(decoration, &KDecoration2::Decoration::bordersChanged, this,
            [this]() {
                GeometryUpdatesBlocker blocker(this);
                RequestGeometryBlocker requestBlocker(this);
                const QRect oldGeometry = frameGeometry();
                if (!isShade()) {
                    checkWorkspacePosition(oldGeometry);
                }
                emit geometryShapeChanged(this, oldGeometry);
            }
        );
    }
    setDecoration(decoration);
    // TODO: ensure the new geometry still fits into the client area (e.g. maximized windows)
    doSetGeometry(QRect(oldGeom.topLeft(), m_windowGeometry.size() + QSize(borderLeft() + borderRight(), borderBottom() + borderTop())));

    emit geometryShapeChanged(this, oldGeom);
}

void XdgShellClient::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force &&
            ((!isDecorated() && noBorder()) || (isDecorated() && !noBorder())))
        return;
    QRect oldgeom = frameGeometry();
    QRect oldClientGeom = oldgeom.adjusted(borderLeft(), borderTop(), -borderRight(), -borderBottom());
    blockGeometryUpdates(true);
    if (force)
        destroyDecoration();
    if (!noBorder()) {
        createDecoration(oldgeom);
    } else
        destroyDecoration();
    if (m_serverDecoration && isDecorated()) {
        m_serverDecoration->setMode(KWayland::Server::ServerSideDecorationManagerInterface::Mode::Server);
    }
    if (m_xdgDecoration) {
        auto mode = isDecorated() || m_userNoBorder ? XdgDecorationInterface::Mode::ServerSide: XdgDecorationInterface::Mode::ClientSide;
        m_xdgDecoration->configure(mode);
        if (m_requestGeometryBlockCounter == 0) {
            m_xdgShellSurface->configure(xdgSurfaceStates(), m_requestedClientSize);
        }
    }
    updateShadow();
    if (check_workspace_pos)
        checkWorkspacePosition(oldgeom, -2, oldClientGeom);
    blockGeometryUpdates(false);
}

void XdgShellClient::setFrameGeometry(int x, int y, int w, int h, ForceGeometry_t force)
{
    const QRect newGeometry = rules()->checkGeometry(QRect(x, y, w, h));

    if (areGeometryUpdatesBlocked()) {
        // when the GeometryUpdateBlocker exits the current geom is passed to setGeometry
        // thus we need to set it here.
        geom = newGeometry;
        if (pendingGeometryUpdate() == PendingGeometryForced) {
            // maximum, nothing needed
        } else if (force == ForceGeometrySet) {
            setPendingGeometryUpdate(PendingGeometryForced);
        } else {
            setPendingGeometryUpdate(PendingGeometryNormal);
        }
        return;
    }

    if (pendingGeometryUpdate() != PendingGeometryNone) {
        // reset geometry to the one before blocking, so that we can compare properly
        geom = geometryBeforeUpdateBlocking();
    }

    const QSize requestedClientSize = newGeometry.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());

    if (requestedClientSize == m_windowGeometry.size() &&
        (m_requestedClientSize.isEmpty() || requestedClientSize == m_requestedClientSize)) {
        // size didn't change, and we don't need to explicitly request a new size
        doSetGeometry(newGeometry);
        updateMaximizeMode(m_requestedMaximizeMode);
    } else {
        // size did change, Client needs to provide a new buffer
        requestGeometry(newGeometry);
    }
}

QRect XdgShellClient::determineBufferGeometry() const
{
    // Offset of the main surface relative to the frame rect.
    const int offsetX = borderLeft() - m_windowGeometry.left();
    const int offsetY = borderTop() - m_windowGeometry.top();

    QRect bufferGeometry;
    bufferGeometry.setX(x() + offsetX);
    bufferGeometry.setY(y() + offsetY);
    bufferGeometry.setSize(surface()->size());

    return bufferGeometry;
}

void XdgShellClient::doSetGeometry(const QRect &rect)
{
    bool frameGeometryIsChanged = false;
    bool bufferGeometryIsChanged = false;

    if (geom != rect) {
        geom = rect;
        frameGeometryIsChanged = true;
    }

    const QRect bufferGeometry = determineBufferGeometry();
    if (m_bufferGeometry != bufferGeometry) {
        m_bufferGeometry = bufferGeometry;
        bufferGeometryIsChanged = true;
    }

    if (!frameGeometryIsChanged && !bufferGeometryIsChanged) {
        return;
    }

    if (m_unmapped && m_geomMaximizeRestore.isEmpty() && !geom.isEmpty()) {
        // use first valid geometry as restore geometry
        m_geomMaximizeRestore = geom;
    }

    if (frameGeometryIsChanged) {
        if (hasStrut()) {
            workspace()->updateClientArea();
        }
        updateWindowRules(Rules::Position | Rules::Size);
    }

    const auto old = geometryBeforeUpdateBlocking();
    addRepaintDuringGeometryUpdates();
    updateGeometryBeforeUpdateBlocking();
    emit geometryShapeChanged(this, old);

    if (isResize()) {
        performMoveResize();
    }
}

void XdgShellClient::doMove(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    m_bufferGeometry = determineBufferGeometry();
}

QByteArray XdgShellClient::windowRole() const
{
    return QByteArray();
}

bool XdgShellClient::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
{
    if (checks.testFlag(SameApplicationCheck::AllowCrossProcesses)) {
        if (other->desktopFileName() == desktopFileName()) {
            return true;
        }
    }
    if (auto s = other->surface()) {
        return s->client() == surface()->client();
    }
    return false;
}

void XdgShellClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)
}

QString XdgShellClient::captionNormal() const
{
    return m_caption;
}

QString XdgShellClient::captionSuffix() const
{
    return m_captionSuffix;
}

void XdgShellClient::updateCaption()
{
    const QString oldSuffix = m_captionSuffix;
    const auto shortcut = shortcutCaptionSuffix();
    m_captionSuffix = shortcut;
    if ((!isSpecialWindow() || isToolbar()) && findClientWithSameCaption()) {
        int i = 2;
        do {
            m_captionSuffix = shortcut + QLatin1String(" <") + QString::number(i) + QLatin1Char('>');
            i++;
        } while (findClientWithSameCaption());
    }
    if (m_captionSuffix != oldSuffix) {
        emit captionChanged();
    }
}

void XdgShellClient::closeWindow()
{
    if (m_xdgShellSurface && isCloseable()) {
        m_xdgShellSurface->close();
        const qint32 pingSerial = static_cast<XdgShellInterface *>(m_xdgShellSurface->global())->ping(m_xdgShellSurface);
        m_pingSerials.insert(pingSerial, PingReason::CloseWindow);
    }
}

AbstractClient *XdgShellClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

bool XdgShellClient::isCloseable() const
{
    if (m_windowType == NET::Desktop || m_windowType == NET::Dock) {
        return false;
    }
    if (m_xdgShellSurface) {
        return true;
    }
    return false;
}

bool XdgShellClient::isFullScreen() const
{
    return m_fullScreen;
}

bool XdgShellClient::isMaximizable() const
{
    if (!isResizable()) {
        return false;
    }
    if (rules()->checkMaximize(MaximizeRestore) != MaximizeRestore || rules()->checkMaximize(MaximizeFull) != MaximizeFull) {
        return false;
    }
    return true;
}

bool XdgShellClient::isMinimizable() const
{
    if (!rules()->checkMinimize(true)) {
        return false;
    }
    return (!m_plasmaShellSurface || m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal);
}

bool XdgShellClient::isMovable() const
{
    if (isFullScreen()) {
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool XdgShellClient::isMovableAcrossScreens() const
{
    if (rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool XdgShellClient::isResizable() const
{
    if (isFullScreen()) {
        return false;
    }
    if (rules()->checkSize(QSize()).isValid()) {
        return false;
    }
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool XdgShellClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !m_closing && !m_unmapped && !isMinimized() && !m_hidden;
}

bool XdgShellClient::isHiddenInternal() const
{
    return m_unmapped || m_hidden;
}

void XdgShellClient::hideClient(bool hide)
{
    if (m_hidden == hide) {
        return;
    }
    m_hidden = hide;
    if (hide) {
        addWorkspaceRepaint(visibleRect());
        workspace()->clientHidden(this);
        emit windowHidden(this);
    } else {
        emit windowShown(this);
    }
}

static bool changeMaximizeRecursion = false;
void XdgShellClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    if (changeMaximizeRecursion) {
        return;
    }

    if (!isResizable()) {
        return;
    }

    const QRect clientArea = isElectricBorderMaximizing() ?
        workspace()->clientArea(MaximizeArea, Cursor::pos(), desktop()) :
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
    RequestGeometryBlocker geometryBlocker(this);

    // call into decoration update borders
    if (isDecorated() && decoration()->client() && !(options->borderlessMaximizedWindows() && m_requestedMaximizeMode == KWin::MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = decoration()->client().data();
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
                !clientArea.contains(m_geomMaximizeRestore.center())) {
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
        m_geomMaximizeRestore = oldGeometry;
        // TODO: Client has more checks
        if (options->electricBorderMaximize()) {
            updateQuickTileMode(QuickTileFlag::Maximize);
        } else {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            emit quickTileModeChanged();
        }
        setFrameGeometry(workspace()->clientArea(MaximizeArea, this));
        workspace()->raiseClient(this);
    } else {
        if (m_requestedMaximizeMode == MaximizeRestore) {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            emit quickTileModeChanged();
        }

        if (m_geomMaximizeRestore.isValid()) {
            setFrameGeometry(m_geomMaximizeRestore);
        } else {
            setFrameGeometry(workspace()->clientArea(PlacementArea, this));
        }
    }
}

void XdgShellClient::setGeometryRestore(const QRect &geo)
{
    m_geomMaximizeRestore = geo;
}

MaximizeMode XdgShellClient::maximizeMode() const
{
    return m_maximizeMode;
}

MaximizeMode XdgShellClient::requestedMaximizeMode() const
{
    return m_requestedMaximizeMode;
}

QRect XdgShellClient::geometryRestore() const
{
    return m_geomMaximizeRestore;
}

bool XdgShellClient::noBorder() const
{
    if (m_serverDecoration) {
        if (m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
            return m_userNoBorder || isFullScreen();
        }
    }
    if (m_xdgDecoration && m_xdgDecoration->requestedMode() != XdgDecorationInterface::Mode::ClientSide) {
        return m_userNoBorder || isFullScreen();
    }
    return true;
}

bool XdgShellClient::isFullScreenable() const
{
    if (!rules()->checkFullScreen(true)) {
        return false;
    }
    return !isSpecialWindow();
}

void XdgShellClient::setFullScreen(bool set, bool user)
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
        workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event
    } else {
        m_geomFsRestore = frameGeometry();
    }
    m_fullScreen = set;

    if (set) {
        workspace()->raiseClient(this);
    }
    RequestGeometryBlocker requestBlocker(this);
    StackingUpdatesBlocker blocker1(workspace());
    GeometryUpdatesBlocker blocker2(this);

    workspace()->updateClientLayer(this);   // active fullscreens get different layer
    updateDecoration(false, false);

    if (set) {
        setFrameGeometry(workspace()->clientArea(FullScreenArea, this));
    } else {
        if (m_geomFsRestore.isValid()) {
            int currentScreen = screen();
            setFrameGeometry(QRect(m_geomFsRestore.topLeft(), adjustedSize(m_geomFsRestore.size())));
            if( currentScreen != screen())
                workspace()->sendClientToScreen( this, currentScreen );
        } else {
            // this can happen when the window was first shown already fullscreen,
            // so let the client set the size by itself
            setFrameGeometry(QRect(workspace()->clientArea(PlacementArea, this).topLeft(), QSize(0, 0)));
        }
    }

    updateWindowRules(Rules::Fullscreen|Rules::Position|Rules::Size);
    emit fullScreenChanged();
}

void XdgShellClient::setNoBorder(bool set)
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

void XdgShellClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)
}

void XdgShellClient::takeFocus()
{
    if (rules()->checkAcceptFocus(wantsInput())) {
        if (m_xdgShellSurface) {
            const qint32 pingSerial = static_cast<XdgShellInterface *>(m_xdgShellSurface->global())->ping(m_xdgShellSurface);
            m_pingSerials.insert(pingSerial, PingReason::FocusWindow);
        }
        setActive(true);
    }

    if (!keepAbove() && !isOnScreenDisplay() && !belongsToDesktop()) {
        workspace()->setShowingDesktop(false);
    }
}

void XdgShellClient::doSetActive()
{
    if (!isActive()) {
        return;
    }
    StackingUpdatesBlocker blocker(workspace());
    workspace()->focusToNull();
}

bool XdgShellClient::userCanSetFullScreen() const
{
    if (m_xdgShellSurface) {
        return true;
    }
    return false;
}

bool XdgShellClient::userCanSetNoBorder() const
{
    if (m_serverDecoration && m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
        return !isFullScreen() && !isShade();
    }
    if (m_xdgDecoration && m_xdgDecoration->requestedMode() != XdgDecorationInterface::Mode::ClientSide) {
        return !isFullScreen() && !isShade();
    }
    return false;
}

bool XdgShellClient::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus());
}

bool XdgShellClient::acceptsFocus() const
{
    if (waylandServer()->inputMethodConnection() == surface()->client()) {
        return false;
    }
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::OnScreenDisplay ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::ToolTip ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Notification ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::CriticalNotification) {
            return false;
        }
    }
    if (m_closing) {
        // a closing window does not accept focus
        return false;
    }
    if (m_unmapped) {
        // an unmapped window does not accept focus
        return false;
    }
    if (m_xdgShellSurface) {
        // TODO: proper
        return true;
    }
    return false;
}

void XdgShellClient::createWindowId()
{
    m_windowId = waylandServer()->createWindowId(surface());
}

pid_t XdgShellClient::pid() const
{
    return surface()->client()->processId();
}

bool XdgShellClient::isLockScreen() const
{
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool XdgShellClient::isInputMethod() const
{
    return surface()->client() == waylandServer()->inputMethodConnection();
}

void XdgShellClient::requestGeometry(const QRect &rect)
{
    if (m_requestGeometryBlockCounter != 0) {
        m_blockedRequestGeometry = rect;
        return;
    }

    QSize size;
    if (rect.isValid()) {
        size = rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    } else {
        size = QSize(0, 0);
    }
    m_requestedClientSize = size;

    quint64 serialId = 0;

    if (m_xdgShellSurface) {
        serialId = m_xdgShellSurface->configure(xdgSurfaceStates(), size);
    }
    if (m_xdgShellPopup) {
        auto parent = transientFor();
        if (parent) {
            const QPoint globalClientContentPos = parent->frameGeometry().topLeft() + parent->clientPos();
            const QPoint relativeOffset = rect.topLeft() - globalClientContentPos;
            serialId = m_xdgShellPopup->configure(QRect(relativeOffset, size));
        }
    }

    if (rect.isValid()) { //if there's no requested size, then there's implicity no positional information worth using
        PendingConfigureRequest configureRequest;
        configureRequest.serialId = serialId;
        configureRequest.positionAfterResize = rect.topLeft();
        configureRequest.maximizeMode = m_requestedMaximizeMode;
        m_pendingConfigureRequests.append(configureRequest);
    }

    m_blockedRequestGeometry = QRect();
}

void XdgShellClient::updatePendingGeometry()
{
    QPoint position = pos();
    MaximizeMode maximizeMode = m_maximizeMode;
    for (auto it = m_pendingConfigureRequests.begin(); it != m_pendingConfigureRequests.end(); it++) {
        if (it->serialId > m_lastAckedConfigureRequest) {
            //this serial is not acked yet, therefore we know all future serials are not
            break;
        }
        if (it->serialId == m_lastAckedConfigureRequest) {
            if (position != it->positionAfterResize) {
                addLayerRepaint(frameGeometry());
            }
            position = it->positionAfterResize;
            maximizeMode = it->maximizeMode;

            m_pendingConfigureRequests.erase(m_pendingConfigureRequests.begin(), ++it);
            break;
        }
        //else serialId < m_lastAckedConfigureRequest and the state is now irrelevant and can be ignored
    }
    doSetGeometry(QRect(position, m_windowGeometry.size() + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    updateMaximizeMode(maximizeMode);
}

void XdgShellClient::handleConfigureAcknowledged(quint32 serial)
{
    m_lastAckedConfigureRequest = serial;
}

void XdgShellClient::handleTransientForChanged()
{
    SurfaceInterface *transientSurface = nullptr;
    if (m_xdgShellSurface) {
        if (auto transient = m_xdgShellSurface->transientFor().data()) {
            transientSurface = transient->surface();
        }
    }
    if (m_xdgShellPopup) {
        transientSurface = m_xdgShellPopup->transientFor().data();
    }
    if (!transientSurface) {
        transientSurface = waylandServer()->findForeignTransientForSurface(surface());
    }
    XdgShellClient *transientClient = waylandServer()->findClient(transientSurface);
    if (transientClient != transientFor()) {
        // Remove from main client.
        if (transientFor()) {
            transientFor()->removeTransient(this);
        }
        setTransientFor(transientClient);
        if (transientClient) {
            transientClient->addTransient(this);
        }
    }
    m_transient = (transientSurface != nullptr);
}

void XdgShellClient::handleWindowClassChanged(const QByteArray &windowClass)
{
    setResourceClass(resourceName(), windowClass);
    if (m_isInitialized && supportsWindowRules()) {
        setupWindowRules(true);
        applyWindowRules();
    }
    setDesktopFileName(windowClass);
}

static QRect subSurfaceTreeRect(const SurfaceInterface *surface, const QPoint &position = QPoint())
{
    QRect rect(position, surface->size());

    const QList<QPointer<SubSurfaceInterface>> subSurfaces = surface->childSubSurfaces();
    for (const QPointer<SubSurfaceInterface> &subSurface : subSurfaces) {
        if (Q_UNLIKELY(!subSurface)) {
            continue;
        }
        const SurfaceInterface *child = subSurface->surface();
        if (Q_UNLIKELY(!child)) {
            continue;
        }
        rect |= subSurfaceTreeRect(child, position + subSurface->position());
    }

    return rect;
}

void XdgShellClient::handleWindowGeometryChanged(const QRect &windowGeometry)
{
    const QRect boundingRect = subSurfaceTreeRect(surface());
    m_windowGeometry = windowGeometry & boundingRect;
    m_hasWindowGeometry = true;
}

void XdgShellClient::handleWindowTitleChanged(const QString &title)
{
    const QString oldSuffix = m_captionSuffix;
    m_caption = title.simplified();
    updateCaption();
    if (m_captionSuffix == oldSuffix) {
        // Don't emit caption change twice it already got emitted by the changing suffix.
        emit captionChanged();
    }
}

void XdgShellClient::handleMoveRequested(SeatInterface *seat, quint32 serial)
{
    // FIXME: Check the seat and serial.
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    performMouseCommand(Options::MouseMove, Cursor::pos());
}

void XdgShellClient::handleResizeRequested(SeatInterface *seat, quint32 serial, Qt::Edges edges)
{
    // FIXME: Check the seat and serial.
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    if (!isResizable() || isShade()) {
        return;
    }
    if (isMoveResize()) {
        finishMoveResize(false);
    }
    setMoveResizePointerButtonDown(true);
    setMoveOffset(Cursor::pos() - pos());  // map from global
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

void XdgShellClient::handleMinimizeRequested()
{
    performMouseCommand(Options::MouseMinimize, Cursor::pos());
}

void XdgShellClient::handleMaximizeRequested(bool maximized)
{
    // If the maximized state of the client hasn't been changed due to a window
    // rule or because the requested state is the same as the current, then the
    // compositor still has to send a configure event.
    RequestGeometryBlocker blocker(this);

    maximize(maximized ? MaximizeFull : MaximizeRestore);
}

void XdgShellClient::handleFullScreenRequested(bool fullScreen, OutputInterface *output)
{
    // FIXME: Consider output as well.
    Q_UNUSED(output);
    setFullScreen(fullScreen, false);
}

void XdgShellClient::handleWindowMenuRequested(SeatInterface *seat, quint32 serial, const QPoint &surfacePos)
{
    // FIXME: Check the seat and serial.
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    performMouseCommand(Options::MouseOperationsMenu, pos() + surfacePos);
}

void XdgShellClient::handleGrabRequested(SeatInterface *seat, quint32 serial)
{
    // FIXME: Check the seat and serial as well whether the parent had focus.
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    m_hasPopupGrab = true;
}

void XdgShellClient::handlePingDelayed(quint32 serial)
{
    auto it = m_pingSerials.find(serial);
    if (it != m_pingSerials.end()) {
        qCDebug(KWIN_CORE) << "First ping timeout:" << caption();
        setUnresponsive(true);
    }
}

void XdgShellClient::handlePingTimeout(quint32 serial)
{
    auto it = m_pingSerials.find(serial);
    if (it != m_pingSerials.end()) {
        if (it.value() == PingReason::CloseWindow) {
            qCDebug(KWIN_CORE) << "Final ping timeout on a close attempt, asking to kill:" << caption();

            //for internal windows, killing the window will delete this
            QPointer<QObject> guard(this);
            killWindow();
            if (!guard) {
                return;
            }
        }
        m_pingSerials.erase(it);
    }
}

void XdgShellClient::handlePongReceived(quint32 serial)
{
    auto it = m_pingSerials.find(serial);
    if (it != m_pingSerials.end()) {
        setUnresponsive(false);
        m_pingSerials.erase(it);
    }
}

void XdgShellClient::handleCommitted()
{
    if (!surface()->buffer()) {
        return;
    }

    if (!m_hasWindowGeometry) {
        m_windowGeometry = subSurfaceTreeRect(surface());
    }

    updatePendingGeometry();

    setDepth((surface()->buffer()->hasAlphaChannel() && !isDesktop()) ? 32 : 24);
    markAsMapped();
}

void XdgShellClient::resizeWithChecks(int w, int h, ForceGeometry_t force)
{
    const QRect area = workspace()->clientArea(WorkArea, this);
    // don't allow growing larger than workarea
    if (w > area.width()) {
        w = area.width();
    }
    if (h > area.height()) {
        h = area.height();
    }
    setFrameGeometry(x(), y(), w, h, force);
}

void XdgShellClient::unmap()
{
    m_unmapped = true;
    if (isMoveResize()) {
        leaveMoveResize();
    }
    m_requestedClientSize = QSize(0, 0);
    destroyWindowManagementInterface();
    if (Workspace::self()) {
        addWorkspaceRepaint(visibleRect());
        workspace()->clientHidden(this);
    }
    emit windowHidden(this);
}

void XdgShellClient::installPlasmaShellSurface(PlasmaShellSurfaceInterface *surface)
{
    m_plasmaShellSurface = surface;
    auto updatePosition = [this, surface] {
        // That's a mis-use of doSetGeometry method. One should instead use move method.
        QRect rect = QRect(surface->position(), size());
        doSetGeometry(rect);
    };
    auto updateRole = [this, surface] {
        NET::WindowType type = NET::Unknown;
        switch (surface->role()) {
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
        if (type != m_windowType) {
            m_windowType = type;
            if (m_windowType == NET::Desktop || type == NET::Dock || type == NET::OnScreenDisplay || type == NET::Notification || type == NET::Tooltip || type == NET::CriticalNotification) {
                setOnAllDesktops(true);
            }
            workspace()->updateClientArea();
        }
    };
    connect(surface, &PlasmaShellSurfaceInterface::positionChanged, this, updatePosition);
    connect(surface, &PlasmaShellSurfaceInterface::roleChanged, this, updateRole);
    connect(surface, &PlasmaShellSurfaceInterface::panelBehaviorChanged, this,
        [this] {
            updateShowOnScreenEdge();
            workspace()->updateClientArea();
        }
    );
    connect(surface, &PlasmaShellSurfaceInterface::panelAutoHideHideRequested, this,
        [this] {
            hideClient(true);
            m_plasmaShellSurface->hideAutoHidingPanel();
            updateShowOnScreenEdge();
        }
    );
    connect(surface, &PlasmaShellSurfaceInterface::panelAutoHideShowRequested, this,
        [this] {
            hideClient(false);
            ScreenEdges::self()->reserve(this, ElectricNone);
            m_plasmaShellSurface->showAutoHidingPanel();
        }
    );
    updatePosition();
    updateRole();
    updateShowOnScreenEdge();
    connect(this, &XdgShellClient::geometryChanged, this, &XdgShellClient::updateShowOnScreenEdge);

    setSkipTaskbar(surface->skipTaskbar());
    connect(surface, &PlasmaShellSurfaceInterface::skipTaskbarChanged, this, [this] {
        setSkipTaskbar(m_plasmaShellSurface->skipTaskbar());
    });

    setSkipSwitcher(surface->skipSwitcher());
    connect(surface, &PlasmaShellSurfaceInterface::skipSwitcherChanged, this, [this] {
        setSkipSwitcher(m_plasmaShellSurface->skipSwitcher());
    });
}

void XdgShellClient::updateShowOnScreenEdge()
{
    if (!ScreenEdges::self()) {
        return;
    }
    if (m_unmapped || !m_plasmaShellSurface || m_plasmaShellSurface->role() != PlasmaShellSurfaceInterface::Role::Panel) {
        ScreenEdges::self()->reserve(this, ElectricNone);
        return;
    }
    if ((m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide && m_hidden) ||
        m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover) {
        // screen edge API requires an edge, thus we need to figure out which edge the window borders
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
        // a panel might border multiple screen edges. E.g. a horizontal panel at the bottom will
        // also border the left and right edge
        // let's remove such cases
        if (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::RightEdge)) {
            edges = edges & (~(Qt::LeftEdge | Qt::RightEdge));
        }
        if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::BottomEdge)) {
            edges = edges & (~(Qt::TopEdge | Qt::BottomEdge));
        }
        // it's still possible that a panel borders two edges, e.g. bottom and left
        // in that case the one which is sharing more with the edge wins
        auto check = [clientGeometry](Qt::Edges edges, Qt::Edge horiz, Qt::Edge vert) {
            if (edges.testFlag(horiz) && edges.testFlag(vert)) {
                if (clientGeometry.width() >= clientGeometry.height()) {
                    return edges & ~horiz;
                } else {
                    return edges & ~vert;
                }
            }
            return edges;
        };
        edges = check(edges, Qt::LeftEdge, Qt::TopEdge);
        edges = check(edges, Qt::LeftEdge, Qt::BottomEdge);
        edges = check(edges, Qt::RightEdge, Qt::TopEdge);
        edges = check(edges, Qt::RightEdge, Qt::BottomEdge);

        ElectricBorder border = ElectricNone;
        if (edges.testFlag(Qt::LeftEdge)) {
            border = ElectricLeft;
        }
        if (edges.testFlag(Qt::RightEdge)) {
            border = ElectricRight;
        }
        if (edges.testFlag(Qt::TopEdge)) {
            border = ElectricTop;
        }
        if (edges.testFlag(Qt::BottomEdge)) {
            border = ElectricBottom;
        }
        ScreenEdges::self()->reserve(this, border);
    } else {
        ScreenEdges::self()->reserve(this, ElectricNone);
    }
}

bool XdgShellClient::isInitialPositionSet() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->isPositionSet();
    }
    return false;
}

void XdgShellClient::installAppMenu(AppMenuInterface *menu)
{
    m_appMenuInterface = menu;

    auto updateMenu = [this](AppMenuInterface::InterfaceAddress address) {
        updateApplicationMenuServiceName(address.serviceName);
        updateApplicationMenuObjectPath(address.objectPath);
    };
    connect(m_appMenuInterface, &AppMenuInterface::addressChanged, this, [=](AppMenuInterface::InterfaceAddress address) {
        updateMenu(address);
    });
    updateMenu(menu->address());
}

void XdgShellClient::installPalette(ServerSideDecorationPaletteInterface *palette)
{
    m_paletteInterface = palette;

    auto updatePalette = [this](const QString &palette) {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(palette));
    };
    connect(m_paletteInterface, &ServerSideDecorationPaletteInterface::paletteChanged, this, [=](const QString &palette) {
        updatePalette(palette);
    });
    connect(m_paletteInterface, &QObject::destroyed, this, [=]() {
        updatePalette(QString());
    });
    updatePalette(palette->palette());
}

void XdgShellClient::updateColorScheme()
{
    if (m_paletteInterface) {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(m_paletteInterface->palette()));
    } else {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(QString()));
    }
}

void XdgShellClient::updateMaximizeMode(MaximizeMode maximizeMode)
{
    if (maximizeMode == m_maximizeMode) {
        return;
    }

    m_maximizeMode = maximizeMode;
    updateWindowRules(Rules::MaximizeHoriz | Rules::MaximizeVert | Rules::Position | Rules::Size);

    emit clientMaximizedStateChanged(this, m_maximizeMode);
    emit clientMaximizedStateChanged(this, m_maximizeMode & MaximizeHorizontal, m_maximizeMode & MaximizeVertical);
}

bool XdgShellClient::hasStrut() const
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

quint32 XdgShellClient::windowId() const
{
    return m_windowId;
}

void XdgShellClient::updateIcon()
{
    const QString waylandIconName = QStringLiteral("wayland");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

bool XdgShellClient::isTransient() const
{
    return m_transient;
}

bool XdgShellClient::hasTransientPlacementHint() const
{
    return isTransient() && transientFor() && m_xdgShellPopup;
}

QRect XdgShellClient::transientPlacement(const QRect &bounds) const
{
    QRect anchorRect;
    Qt::Edges anchorEdge;
    Qt::Edges gravity;
    QPoint offset;
    PositionerConstraints constraintAdjustments;
    QSize size = frameGeometry().size();

    const QPoint parentClientPos = transientFor()->pos() + transientFor()->clientPos();
    QRect popupPosition;

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

    if (m_xdgShellPopup) {
        anchorRect = m_xdgShellPopup->anchorRect();
        anchorEdge = m_xdgShellPopup->anchorEdge();
        gravity = m_xdgShellPopup->gravity();
        offset = m_xdgShellPopup->anchorOffset();
        constraintAdjustments = m_xdgShellPopup->constraintAdjustments();
        if (!size.isValid()) {
            size = m_xdgShellPopup->initialSize();
        }
    } else {
        Q_UNREACHABLE();
    }


    //initial position
    popupPosition = QRect(popupOffset(anchorRect, anchorEdge, gravity, size) + offset + parentClientPos, size);

    //if that fits, we don't need to do anything
    if (inBounds(popupPosition)) {
        return popupPosition;
    }
    //otherwise apply constraint adjustment per axis in order XDG Shell Popup states

    if (constraintAdjustments & PositionerConstraint::FlipX) {
        if (!inBounds(popupPosition, Qt::LeftEdge | Qt::RightEdge)) {
            //flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = anchorEdge;
            if (flippedAnchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedAnchorEdge ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedGravity = gravity;
            if (flippedGravity & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedGravity ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedPopupPosition = QRect(popupOffset(anchorRect, flippedAnchorEdge, flippedGravity, size) + offset + parentClientPos, size);

            //if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupPosition, Qt::LeftEdge | Qt::RightEdge)) {
                popupPosition.moveLeft(flippedPopupPosition.x());
            }
        }
    }
    if (constraintAdjustments & PositionerConstraint::SlideX) {
        if (!inBounds(popupPosition, Qt::LeftEdge)) {
            popupPosition.moveLeft(bounds.x());
        }
        if (!inBounds(popupPosition, Qt::RightEdge)) {
            // moveRight suffers from the classic QRect off by one issue
            popupPosition.moveLeft(bounds.x() + bounds.width() - size.width());
        }
    }
    if (constraintAdjustments & PositionerConstraint::ResizeX) {
        //TODO
        //but we need to sort out when this is run as resize should only happen before first configure
    }

    if (constraintAdjustments & PositionerConstraint::FlipY) {
        if (!inBounds(popupPosition, Qt::TopEdge | Qt::BottomEdge)) {
            //flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = anchorEdge;
            if (flippedAnchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedAnchorEdge ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedGravity = gravity;
            if (flippedGravity & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedGravity ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedPopupPosition = QRect(popupOffset(anchorRect, flippedAnchorEdge, flippedGravity, size) + offset + parentClientPos, size);

            //if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupPosition, Qt::TopEdge | Qt::BottomEdge)) {
                popupPosition.moveTop(flippedPopupPosition.y());
            }
        }
    }
    if (constraintAdjustments & PositionerConstraint::SlideY) {
        if (!inBounds(popupPosition, Qt::TopEdge)) {
            popupPosition.moveTop(bounds.y());
        }
        if (!inBounds(popupPosition, Qt::BottomEdge)) {
            popupPosition.moveTop(bounds.y() + bounds.height() - size.height());
        }
    }
    if (constraintAdjustments & PositionerConstraint::ResizeY) {
        //TODO
    }

    return popupPosition;
}

QPoint XdgShellClient::popupOffset(const QRect &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity, const QSize popupSize) const
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

void XdgShellClient::doResizeSync()
{
    requestGeometry(moveResizeGeometry());
}

QMatrix4x4 XdgShellClient::inputTransformation() const
{
    QMatrix4x4 matrix;
    matrix.translate(-m_bufferGeometry.x(), -m_bufferGeometry.y());
    return matrix;
}

void XdgShellClient::installServerSideDecoration(KWayland::Server::ServerSideDecorationInterface *deco)
{
    if (m_serverDecoration == deco) {
        return;
    }
    m_serverDecoration = deco;
    connect(m_serverDecoration, &ServerSideDecorationInterface::destroyed, this,
        [this] {
            m_serverDecoration = nullptr;
            if (m_closing || !Workspace::self()) {
                return;
            }
            if (!m_unmapped) {
                // maybe delay to next event cycle in case the XdgShellClient is getting destroyed, too
                updateDecoration(true);
            }
        }
    );
    if (!m_unmapped) {
        updateDecoration(true);
    }
    connect(m_serverDecoration, &ServerSideDecorationInterface::modeRequested, this,
        [this] (ServerSideDecorationManagerInterface::Mode mode) {
            const bool changed = mode != m_serverDecoration->mode();
            if (changed && !m_unmapped) {
                updateDecoration(false);
            }
        }
    );
}

void XdgShellClient::installXdgDecoration(XdgDecorationInterface *deco)
{
    Q_ASSERT(m_xdgShellSurface);

    m_xdgDecoration = deco;

    connect(m_xdgDecoration, &QObject::destroyed, this,
        [this] {
            m_xdgDecoration = nullptr;
            if (m_closing || !Workspace::self()) {
                return;
            }
            updateDecoration(true);
        }
    );

    connect(m_xdgDecoration, &XdgDecorationInterface::modeRequested, this,
        [this] () {
        //force is true as we must send a new configure response
        updateDecoration(false, true);
    });
}

bool XdgShellClient::shouldExposeToWindowManagement()
{
    if (isLockScreen()) {
        return false;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

KWayland::Server::XdgShellSurfaceInterface::States XdgShellClient::xdgSurfaceStates() const
{
    XdgShellSurfaceInterface::States states;
    if (isActive()) {
        states |= XdgShellSurfaceInterface::State::Activated;
    }
    if (isFullScreen()) {
        states |= XdgShellSurfaceInterface::State::Fullscreen;
    }
    if (m_requestedMaximizeMode == MaximizeMode::MaximizeFull) {
        states |= XdgShellSurfaceInterface::State::Maximized;
    }
    if (isResize()) {
        states |= XdgShellSurfaceInterface::State::Resizing;
    }
    return states;
}

void XdgShellClient::doMinimize()
{
    if (isMinimized()) {
        workspace()->clientHidden(this);
    } else {
        emit windowShown(this);
    }
    workspace()->updateMinimizedOfTransients(this);
}

void XdgShellClient::placeIn(const QRect &area)
{
    Placement::self()->place(this, area);
    setGeometryRestore(frameGeometry());
}

void XdgShellClient::showOnScreenEdge()
{
    if (!m_plasmaShellSurface || m_unmapped) {
        return;
    }
    hideClient(false);
    workspace()->raiseClient(this);
    if (m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide) {
        m_plasmaShellSurface->showAutoHidingPanel();
    }
}

bool XdgShellClient::dockWantsInput() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Panel) {
            return m_plasmaShellSurface->panelTakesFocus();
        }
    }
    return false;
}

void XdgShellClient::killWindow()
{
    if (!surface()) {
        return;
    }
    auto c = surface()->client();
    if (c->processId() == getpid() || c->processId() == 0) {
        c->destroy();
        return;
    }
    ::kill(c->processId(), SIGTERM);
    // give it time to terminate and only if terminate fails, try destroy Wayland connection
    QTimer::singleShot(5000, c, &ClientConnection::destroy);
}

bool XdgShellClient::isLocalhost() const
{
    return true;
}

bool XdgShellClient::hasPopupGrab() const
{
    return m_hasPopupGrab;
}

void XdgShellClient::popupDone()
{
    if (m_xdgShellPopup) {
        m_xdgShellPopup->popupDone();
    }
}

void XdgShellClient::updateClientOutputs()
{
    QVector<OutputInterface*> clientOutputs;
    const auto outputs = waylandServer()->display()->outputs();
    for (OutputInterface* output: qAsConst(outputs)) {
        const QRect outputGeom(output->globalPosition(), output->pixelSize() / output->scale());
        if (frameGeometry().intersects(outputGeom)) {
            clientOutputs << output;
        }
    }
    surface()->setOutputs(clientOutputs);
}

bool XdgShellClient::isPopupWindow() const
{
    if (Toplevel::isPopupWindow()) {
        return true;
    }
    if (m_xdgShellPopup != nullptr) {
        return true;
    }
    return false;
}

bool XdgShellClient::supportsWindowRules() const
{
    if (m_plasmaShellSurface) {
        return false;
    }
    return m_xdgShellSurface;
}

}
