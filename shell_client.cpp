/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>

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
#include "shell_client.h"
#include "composite.h"
#include "cursor.h"
#include "deleted.h"
#include "placement.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "virtualdesktops.h"
#include "screens.h"
#include "decorations/decorationbridge.h"
#include "decorations/decoratedclient.h"
#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <KWayland/Client/surface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/plasmashell_interface.h>
#include <KWayland/Server/shadow_interface.h>
#include <KWayland/Server/server_decoration_interface.h>
#include <KWayland/Server/qtsurfaceextension_interface.h>
#include <KWayland/Server/plasmawindowmanagement_interface.h>
#include <KWayland/Server/appmenu_interface.h>
#include <KWayland/Server/server_decoration_palette_interface.h>
#include <KWayland/Server/xdgdecoration_interface.h>

#include <KDesktopFile>

#include <QFileInfo>
#include <QOpenGLFramebufferObject>
#include <QWindow>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

Q_DECLARE_METATYPE(NET::WindowType)

using namespace KWayland::Server;

namespace KWin
{

ShellClient::ShellClient(ShellSurfaceInterface *surface)
    : AbstractClient()
    , m_shellSurface(surface)
    , m_xdgShellSurface(nullptr)
    , m_xdgShellPopup(nullptr)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    init();
}

ShellClient::ShellClient(XdgShellSurfaceInterface *surface)
    : AbstractClient()
    , m_shellSurface(nullptr)
    , m_xdgShellSurface(surface)
    , m_xdgShellPopup(nullptr)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    m_requestGeometryBlockCounter++;
    init();
    connect(surface->surface(), &SurfaceInterface::committed, this, &ShellClient::finishInit);
}

ShellClient::ShellClient(XdgShellPopupInterface *surface)
    : AbstractClient()
    , m_shellSurface(nullptr)
    , m_xdgShellSurface(nullptr)
    , m_xdgShellPopup(surface)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    m_requestGeometryBlockCounter++;
    init();
    connect(surface->surface(), &SurfaceInterface::committed, this, &ShellClient::finishInit);
}

ShellClient::~ShellClient() = default;

template <class T>
void ShellClient::initSurface(T *shellSurface)
{
    m_caption = shellSurface->title().simplified();
    // delay till end of init
    QTimer::singleShot(0, this, &ShellClient::updateCaption);
    connect(shellSurface, &T::destroyed, this, &ShellClient::destroyClient);
    connect(shellSurface, &T::titleChanged, this,
        [this] (const QString &s) {
            const auto oldSuffix = m_captionSuffix;
            m_caption = s.simplified();
            updateCaption();
            if (m_captionSuffix == oldSuffix) {
                // don't emit caption change twice
                // it already got emitted by the changing suffix
                emit captionChanged();
            }
        }
    );
    connect(shellSurface, &T::moveRequested, this,
        [this] {
            // TODO: check the seat and serial
            performMouseCommand(Options::MouseMove, Cursor::pos());
        }
    );

    // determine the resource name, this is inspired from ICCCM 4.1.2.5
    // the binary name of the invoked client
    QFileInfo info{shellSurface->client()->executablePath()};
    QByteArray resourceName;
    if (info.exists()) {
        resourceName = info.fileName().toUtf8();
    }
    setResourceClass(resourceName, shellSurface->windowClass());
    connect(shellSurface, &T::windowClassChanged, this,
        [this, resourceName] (const QByteArray &windowClass) {
            setResourceClass(resourceName, windowClass);
            if (!m_internal) {
                setupWindowRules(true);
                applyWindowRules();
            }
            setDesktopFileName(windowClass);
        }
    );
    connect(shellSurface, &T::resizeRequested, this,
        [this] (SeatInterface *seat, quint32 serial, Qt::Edges edges) {
            // TODO: check the seat and serial
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
                Position pos = PositionCenter;
                if (edges.testFlag(Qt::TopEdge)) {
                    pos = PositionTop;
                } else if (edges.testFlag(Qt::BottomEdge)) {
                    pos = PositionBottom;
                }
                if (edges.testFlag(Qt::LeftEdge)) {
                    pos = Position(pos | PositionLeft);
                } else if (edges.testFlag(Qt::RightEdge)) {
                    pos = Position(pos | PositionRight);
                }
                return pos;
            };
            setMoveResizePointerMode(toPosition());
            if (!startMoveResize())
                setMoveResizePointerButtonDown(false);
            updateCursor();
        }
    );
    connect(shellSurface, &T::maximizedChanged, this,
        [this] (bool maximized) {
            if (m_shellSurface && isFullScreen()) {
                // ignore for wl_shell - there it is mutual exclusive and messes with the geometry
                return;
            }
            maximize(maximized ? MaximizeFull : MaximizeRestore);
        }
    );
    // TODO: consider output!
    connect(shellSurface, &T::fullscreenChanged, this, &ShellClient::clientFullScreenChanged);

    connect(shellSurface, &T::transientForChanged, this, &ShellClient::setTransient);

    connect(this, &ShellClient::geometryChanged, this, &ShellClient::updateClientOutputs);
    connect(screens(), &Screens::changed, this, &ShellClient::updateClientOutputs);

    if (!m_internal) {
        setupWindowRules(false);
    }
    setDesktopFileName(rules()->checkDesktopFile(shellSurface->windowClass(), true).toUtf8());
}

void ShellClient::init()
{
    connect(this, &ShellClient::desktopFileNameChanged, this, &ShellClient::updateIcon);
    createWindowId();
    setupCompositing();
    updateIcon();
    SurfaceInterface *s = surface();
    Q_ASSERT(s);
    if (s->buffer()) {
        setReadyForPainting();
        if (shouldExposeToWindowManagement()) {
            setupWindowManagementInterface();
        }
        m_unmapped = false;
        m_clientSize = s->size();
    } else {
        ready_for_painting = false;
    }
    if (!m_internal) {
        doSetGeometry(QRect(QPoint(0, 0), m_clientSize));
    }
    if (waylandServer()->inputMethodConnection() == s->client()) {
        m_windowType = NET::OnScreenDisplay;
    }

    connect(s, &SurfaceInterface::sizeChanged, this,
        [this] {
            m_clientSize = surface()->size();
            doSetGeometry(QRect(geom.topLeft(), m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
        }
    );
    connect(s, &SurfaceInterface::unmapped, this, &ShellClient::unmap);
    connect(s, &SurfaceInterface::unbound, this, &ShellClient::destroyClient);
    connect(s, &SurfaceInterface::destroyed, this, &ShellClient::destroyClient);
    if (m_shellSurface) {
        initSurface(m_shellSurface);
        auto setPopup = [this] {
            // TODO: verify grab serial
            m_hasPopupGrab = m_shellSurface->isPopup();
        };
        connect(m_shellSurface, &ShellSurfaceInterface::popupChanged, this, setPopup);
        setPopup();
    } else if (m_xdgShellSurface) {
        initSurface(m_xdgShellSurface);

        auto global = static_cast<XdgShellInterface *>(m_xdgShellSurface->global());
        connect(global, &XdgShellInterface::pingDelayed,
            this, [this](qint32 serial) {
                auto it = m_pingSerials.find(serial);
                if (it != m_pingSerials.end()) {
                    qCDebug(KWIN_CORE) << "First ping timeout:" << caption();
                    setUnresponsive(true);
                }
            });

        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::configureAcknowledged, this, [this](int serial) {
           m_lastAckedConfigureRequest = serial;
        });

        connect(global, &XdgShellInterface::pingTimeout,
            this, [this](qint32 serial) {
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
            });

        connect(global, &XdgShellInterface::pongReceived,
            this, [this](qint32 serial){
                auto it = m_pingSerials.find(serial);
                if (it != m_pingSerials.end()) {
                    setUnresponsive(false);
                    m_pingSerials.erase(it);
                }
            });

        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::windowMenuRequested, this,
            [this] (SeatInterface *seat, quint32 serial, const QPoint &surfacePos) {
                // TODO: check serial on seat
                Q_UNUSED(seat)
                Q_UNUSED(serial)
                performMouseCommand(Options::MouseOperationsMenu, pos() + surfacePos);
            }
        );
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::minimizeRequested, this,
            [this] {
                performMouseCommand(Options::MouseMinimize, Cursor::pos());
            }
        );
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
    } else if (m_xdgShellPopup) {
        connect(m_xdgShellPopup, &XdgShellPopupInterface::grabRequested, this, [this](SeatInterface *seat, quint32 serial) {
            Q_UNUSED(seat)
            Q_UNUSED(serial)
            //TODO - should check the parent had focus
            m_hasPopupGrab = true;
        });

        connect(m_xdgShellPopup, &XdgShellPopupInterface::configureAcknowledged, this, [this](int serial) {
           m_lastAckedConfigureRequest = serial;
        });

        connect(m_xdgShellPopup, &XdgShellPopupInterface::destroyed, this, &ShellClient::destroyClient);
    }

    // set initial desktop
    setDesktop(rules()->checkDesktop(m_internal ? int(NET::OnAllDesktops) : VirtualDesktopManager::self()->current(), true));
    // TODO: merge in checks from Client::manage?
    if (rules()->checkMinimize(false, true)) {
        minimize(true);   // No animation
    }
    setSkipTaskbar(rules()->checkSkipTaskbar(m_plasmaShellSurface ? m_plasmaShellSurface->skipTaskbar() : false, true));
    setSkipPager(rules()->checkSkipPager(false, true));
    setSkipSwitcher(rules()->checkSkipSwitcher(false, true));
    setKeepAbove(rules()->checkKeepAbove(false, true));
    setKeepBelow(rules()->checkKeepBelow(false, true));
    setShortcut(rules()->checkShortcut(QString(), true));

    // setup shadow integration
    getShadow();
    connect(s, &SurfaceInterface::shadowChanged, this, &Toplevel::getShadow);

    connect(waylandServer(), &WaylandServer::foreignTransientChanged, this, [this](KWayland::Server::SurfaceInterface *child) {
        if (child == surface()) {
            setTransient();
        }
    });
    setTransient();

    AbstractClient::updateColorScheme(QString());

    if (!m_internal) {
        discardTemporaryRules();
        applyWindowRules(); // Just in case
        RuleBook::self()->discardUsed(this, false);   // Remove ApplyNow rules
        updateWindowRules(Rules::All); // Was blocked while !isManaged()
    }
}

void ShellClient::finishInit() {
    SurfaceInterface *s = surface();
    disconnect(s, &SurfaceInterface::committed, this, &ShellClient::finishInit);

    updateWindowMargins();
    if (!isInitialPositionSet()) {
        QRect area = workspace()->clientArea(PlacementArea, Screens::self()->current(), desktop());
        placeIn(area);
    }

    m_requestGeometryBlockCounter--;
    if (m_requestGeometryBlockCounter == 0) {
        requestGeometry(m_blockedRequestGeometry);
    }
}

void ShellClient::destroyClient()
{
    m_closing = true;
    if (isMoveResize()) {
        leaveMoveResize();
    }
    Deleted *del = nullptr;
    if (workspace()) {
        del = Deleted::create(this);
    }
    emit windowClosed(this, del);
    destroyWindowManagementInterface();
    destroyDecoration();

    if (workspace()) {
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
    }
    waylandServer()->removeClient(this);

    if (del) {
        del->unrefWindow();
    }
    m_shellSurface = nullptr;
    m_xdgShellSurface = nullptr;
    m_xdgShellPopup = nullptr;
    deleteClient(this);
}

void ShellClient::deleteClient(ShellClient *c)
{
    delete c;
}

QSize ShellClient::toWindowGeometry(const QSize &size) const
{
    QSize adjustedSize = size - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    // a client going fullscreen should have the window the contents size of the screen
    if (!isFullScreen() && requestedMaximizeMode() != MaximizeFull) {
        adjustedSize -= QSize(m_windowMargins.left() + m_windowMargins.right(), m_windowMargins.top() + m_windowMargins.bottom());
    }
    return adjustedSize;
}

QStringList ShellClient::activities() const
{
    // TODO: implement
    return QStringList();
}

QPoint ShellClient::clientContentPos() const
{
    return -1 * clientPos();
}

QSize ShellClient::clientSize() const
{
    return m_clientSize;
}

void ShellClient::debug(QDebug &stream) const
{
    stream.nospace();
    stream << "\'ShellClient:" << surface() << ";WMCLASS:" << resourceClass() << ":"
           << resourceName() << ";Caption:" << caption() << "\'";
}

bool ShellClient::belongsToDesktop() const
{
    const auto clients = waylandServer()->clients();

    return std::any_of(clients.constBegin(), clients.constEnd(),
        [this](const ShellClient *client) {
            if (belongsToSameApplication(client, SameApplicationChecks())) {
                return client->isDesktop();
            }
            return false;
        }
    );
}

Layer ShellClient::layerForDock() const
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

QRect ShellClient::transparentRect() const
{
    // TODO: implement
    return QRect();
}

NET::WindowType ShellClient::windowType(bool direct, int supported_types) const
{
    // TODO: implement
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

double ShellClient::opacity() const
{
    return m_opacity;
}

void ShellClient::setOpacity(double opacity)
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

void ShellClient::addDamage(const QRegion &damage)
{
    auto s = surface();
    if (s->size().isValid()) {
        m_clientSize = s->size();
        updateWindowMargins();
        updatePendingGeometry();
    }
    markAsMapped();
    setDepth((s->buffer()->hasAlphaChannel() && !isDesktop()) ? 32 : 24);
    repaints_region += damage.translated(clientPos());
    Toplevel::addDamage(damage);
}

void ShellClient::markAsMapped()
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

void ShellClient::createDecoration(const QRect &oldGeom)
{
    KDecoration2::Decoration *decoration = Decoration::DecorationBridge::self()->createDecoration(this);
    if (decoration) {
        QMetaObject::invokeMethod(decoration, "update", Qt::QueuedConnection);
        connect(decoration, &KDecoration2::Decoration::shadowChanged, this, &Toplevel::getShadow);
        connect(decoration, &KDecoration2::Decoration::bordersChanged, this,
            [this]() {
                GeometryUpdatesBlocker blocker(this);
                RequestGeometryBlocker requestBlocker(this);
                QRect oldgeom = geometry();
                if (!isShade())
                    checkWorkspacePosition(oldgeom);
                emit geometryShapeChanged(this, oldgeom);
            }
        );
    }
    setDecoration(decoration);
    // TODO: ensure the new geometry still fits into the client area (e.g. maximized windows)
    doSetGeometry(QRect(oldGeom.topLeft(), m_clientSize + (decoration ? QSize(decoration->borderLeft() + decoration->borderRight(),
                                                               decoration->borderBottom() + decoration->borderTop()) : QSize())));

    emit geometryShapeChanged(this, oldGeom);
}

void ShellClient::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force &&
            ((!isDecorated() && noBorder()) || (isDecorated() && !noBorder())))
        return;
    QRect oldgeom = geometry();
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
    getShadow();
    if (check_workspace_pos)
        checkWorkspacePosition(oldgeom, -2, oldClientGeom);
    blockGeometryUpdates(false);
}

void ShellClient::setGeometry(int x, int y, int w, int h, ForceGeometry_t force)
{
    if (areGeometryUpdatesBlocked()) {
        // when the GeometryUpdateBlocker exits the current geom is passed to setGeometry
        // thus we need to set it here.
        geom = QRect(x, y, w, h);
        if (pendingGeometryUpdate() == PendingGeometryForced)
            {} // maximum, nothing needed
        else if (force == ForceGeometrySet)
            setPendingGeometryUpdate(PendingGeometryForced);
        else
            setPendingGeometryUpdate(PendingGeometryNormal);
        return;
    }
    if (pendingGeometryUpdate() != PendingGeometryNone) {
        // reset geometry to the one before blocking, so that we can compare properly
        geom = geometryBeforeUpdateBlocking();
    }
    const QSize requestedClientSize = QSize(w, h) - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    const QSize requestedWindowGeometrySize = toWindowGeometry(QSize(w, h));

    if (requestedClientSize == m_clientSize && !isWaitingForMoveResizeSync() &&
        (m_requestedClientSize.isEmpty() || requestedWindowGeometrySize == m_requestedClientSize)) {
        // size didn't change, and we don't need to explicitly request a new size
        doSetGeometry(QRect(x, y, w, h));
        updateMaximizeMode(m_requestedMaximizeMode);
    } else {
        // size did change, Client needs to provide a new buffer
        requestGeometry(QRect(x, y, w, h));
    }
}

void ShellClient::doSetGeometry(const QRect &rect)
{
    if (geom == rect && pendingGeometryUpdate() == PendingGeometryNone) {
        return;
    }
    if (!m_unmapped) {
        addWorkspaceRepaint(visibleRect());
    }
    geom = rect;

    if (m_unmapped && m_geomMaximizeRestore.isEmpty() && !geom.isEmpty()) {
        // use first valid geometry as restore geometry
        m_geomMaximizeRestore = geom;
    }

    if (!m_unmapped) {
        addWorkspaceRepaint(visibleRect());
    }
    if (hasStrut()) {
        workspace()->updateClientArea();
    }
    const auto old = geometryBeforeUpdateBlocking();
    updateGeometryBeforeUpdateBlocking();
    emit geometryShapeChanged(this, old);

    if (isResize()) {
        performMoveResize();
    }
}

QByteArray ShellClient::windowRole() const
{
    return QByteArray();
}

bool ShellClient::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
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

void ShellClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)
}

void ShellClient::updateCaption()
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

void ShellClient::closeWindow()
{
    if (m_xdgShellSurface && isCloseable()) {
        m_xdgShellSurface->close();
        const qint32 pingSerial = static_cast<XdgShellInterface *>(m_xdgShellSurface->global())->ping(m_xdgShellSurface);
        m_pingSerials.insert(pingSerial, PingReason::CloseWindow);
    }
}

AbstractClient *ShellClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

bool ShellClient::isCloseable() const
{
    if (m_windowType == NET::Desktop || m_windowType == NET::Dock) {
        return false;
    }
    if (m_xdgShellSurface) {
        return true;
    }
    return false;
}

bool ShellClient::isFullScreen() const
{
    return m_fullScreen;
}

bool ShellClient::isMaximizable() const
{
    return true;
}

bool ShellClient::isMinimizable() const
{
    return (!m_plasmaShellSurface || m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal);
}

bool ShellClient::isMovable() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool ShellClient::isMovableAcrossScreens() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool ShellClient::isResizable() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool ShellClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !m_closing && !m_unmapped && !isMinimized() && !m_hidden;
}

void ShellClient::hideClient(bool hide)
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
void ShellClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
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
    const QRect oldGeometry = geometry();

    StackingUpdatesBlocker blocker(workspace());
    RequestGeometryBlocker geometryBlocker(this);
    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if (!adjust) {
        if (vertical)
            m_requestedMaximizeMode = MaximizeMode(m_requestedMaximizeMode ^ MaximizeVertical);
        if (horizontal)
            m_requestedMaximizeMode = MaximizeMode(m_requestedMaximizeMode ^ MaximizeHorizontal);
    }
    // TODO: add more checks as in Client

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

    // TODO: check rules
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
        setGeometry(workspace()->clientArea(MaximizeArea, this));
        workspace()->raiseClient(this);
    } else {
        if (m_requestedMaximizeMode == MaximizeRestore) {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            emit quickTileModeChanged();
        }

        if (m_geomMaximizeRestore.isValid()) {
            setGeometry(m_geomMaximizeRestore);
        } else {
            setGeometry(workspace()->clientArea(PlacementArea, this));
        }
    }
}

MaximizeMode ShellClient::maximizeMode() const
{
    return m_maximizeMode;
}

MaximizeMode ShellClient::requestedMaximizeMode() const
{
    return m_requestedMaximizeMode;
}

bool ShellClient::noBorder() const
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

void ShellClient::setFullScreen(bool set, bool user)
{
    if (!isFullScreen() && !set)
        return;
    if (user && !userCanSetFullScreen())
        return;
    set = rules()->checkFullScreen(set && !isSpecialWindow());
    setShade(ShadeNone);
    bool was_fs = isFullScreen();
    if (was_fs) {
        workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event
    } else {
        // in shell surface, maximise mode and fullscreen are exclusive
        // fullscreen->toplevel should restore the state we had before maximising
        if (m_shellSurface && m_maximizeMode == MaximizeMode::MaximizeFull) {
            m_geomFsRestore = m_geomMaximizeRestore;
        } else {
            m_geomFsRestore = geometry();
        }
    }
    m_fullScreen = set;
    if (was_fs == isFullScreen())
        return;
    if (set) {
        untab();
        workspace()->raiseClient(this);
    }
    RequestGeometryBlocker requestBlocker(this);
    StackingUpdatesBlocker blocker1(workspace());
    GeometryUpdatesBlocker blocker2(this);
    workspace()->updateClientLayer(this);   // active fullscreens get different layer
    updateDecoration(false, false);
    if (isFullScreen()) {
        setGeometry(workspace()->clientArea(FullScreenArea, this));
    } else {
        if (m_geomFsRestore.isValid()) {
            int currentScreen = screen();
            setGeometry(QRect(m_geomFsRestore.topLeft(), adjustedSize(m_geomFsRestore.size())));
            if( currentScreen != screen())
                workspace()->sendClientToScreen( this, currentScreen );
        } else {
            // this can happen when the window was first shown already fullscreen,
            // so let the client set the size by itself
            setGeometry(QRect(workspace()->clientArea(PlacementArea, this).topLeft(), QSize(0, 0)));
        }
    }
    updateWindowRules(Rules::Fullscreen|Rules::Position|Rules::Size);

    if (was_fs != isFullScreen()) {
        emit fullScreenChanged();
    }
}

void ShellClient::setNoBorder(bool set)
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

void ShellClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)
}

void ShellClient::takeFocus()
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

void ShellClient::doSetActive()
{
    if (!isActive()) {
        return;
    }
    StackingUpdatesBlocker blocker(workspace());
    workspace()->focusToNull();
}

bool ShellClient::userCanSetFullScreen() const
{
    if (m_xdgShellSurface) {
        return true;
    }
    return false;
}

bool ShellClient::userCanSetNoBorder() const
{
    if (m_serverDecoration && m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
        return !isFullScreen() && !isShade() && !tabGroup();
    }
    if (m_xdgDecoration && m_xdgDecoration->requestedMode() != XdgDecorationInterface::Mode::ClientSide) {
        return !isFullScreen() && !isShade() && !tabGroup();
    }
    return false;
}

bool ShellClient::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus());
}

bool ShellClient::acceptsFocus() const
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
    if (m_shellSurface) {
        if (m_shellSurface->isPopup()) {
            return false;
        }
        return m_shellSurface->acceptsKeyboardFocus();
    }
    if (m_xdgShellSurface) {
        // TODO: proper
        return true;
    }
    return false;
}

void ShellClient::createWindowId()
{
    if (!m_internal) {
        m_windowId = waylandServer()->createWindowId(surface());
    }
}

pid_t ShellClient::pid() const
{
    return surface()->client()->processId();
}

bool ShellClient::isLockScreen() const
{
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool ShellClient::isInputMethod() const
{
    return surface()->client() == waylandServer()->inputMethodConnection();
}

bool ShellClient::requestGeometry(const QRect &rect)
{
    if (m_requestGeometryBlockCounter != 0) {
        m_blockedRequestGeometry = rect;
        return false;
    }

    QSize size;
    if (rect.isValid()) {
        size = toWindowGeometry(rect.size());
    } else {
        size = QSize(0, 0);
    }
    m_requestedClientSize = size;

    quint64 serialId = 0;

    if (m_shellSurface && !size.isEmpty()) {
        m_shellSurface->requestSize(size);
    }
    if (m_xdgShellSurface) {
        serialId = m_xdgShellSurface->configure(xdgSurfaceStates(), size);
    }
    if (m_xdgShellPopup) {
        auto parent = transientFor();
        if (parent) {
            const QPoint globalClientContentPos = parent->geometry().topLeft() + parent->clientPos();
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
    return true;
}

void ShellClient::updatePendingGeometry()
{
    QPoint position = geom.topLeft();
    MaximizeMode maximizeMode = m_maximizeMode;
    for (auto it = m_pendingConfigureRequests.begin(); it != m_pendingConfigureRequests.end(); it++) {
        if (it->serialId > m_lastAckedConfigureRequest) {
            //this serial is not acked yet, therefore we know all future serials are not
            break;
        }
        if (it->serialId == m_lastAckedConfigureRequest) {
            if (position != it->positionAfterResize) {
                addLayerRepaint(geometry());
            }
            position = it->positionAfterResize;
            maximizeMode = it->maximizeMode;

            m_pendingConfigureRequests.erase(m_pendingConfigureRequests.begin(), ++it);
            break;
        }
        //else serialId < m_lastAckedConfigureRequest and the state is now irrelevant and can be ignored
    }
    doSetGeometry(QRect(position, m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    updateMaximizeMode(maximizeMode);
}

void ShellClient::clientFullScreenChanged(bool fullScreen)
{
    setFullScreen(fullScreen, false);
}

void ShellClient::resizeWithChecks(int w, int h, ForceGeometry_t force)
{
    Q_UNUSED(force)
    QRect area = workspace()->clientArea(WorkArea, this);
    // don't allow growing larger than workarea
    if (w > area.width()) {
        w = area.width();
    }
    if (h > area.height()) {
        h = area.height();
    }
    if (m_shellSurface) {
        m_shellSurface->requestSize(QSize(w, h));
    }
    if (m_xdgShellSurface) {
        m_xdgShellSurface->configure(xdgSurfaceStates(), QSize(w, h));
    }
}

void ShellClient::unmap()
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

void ShellClient::installPlasmaShellSurface(PlasmaShellSurfaceInterface *surface)
{
    m_plasmaShellSurface = surface;
    auto updatePosition = [this, surface] {
        QRect rect = QRect(surface->position(), m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
        // Shell surfaces of internal windows are sometimes desync to current value.
        // Make sure to not set window geometry of internal windows to invalid values (bug 386304).
        // This is a workaround.
        if (!m_internal || rect.isValid()) {
            doSetGeometry(rect);
        }
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
    connect(this, &ShellClient::geometryChanged, this, &ShellClient::updateShowOnScreenEdge);

    setSkipTaskbar(surface->skipTaskbar());
    connect(surface, &PlasmaShellSurfaceInterface::skipTaskbarChanged, this, [this] {
        setSkipTaskbar(m_plasmaShellSurface->skipTaskbar());
    });

    setSkipSwitcher(surface->skipSwitcher());
    connect(surface, &PlasmaShellSurfaceInterface::skipSwitcherChanged, this, [this] {
        setSkipSwitcher(m_plasmaShellSurface->skipSwitcher());
    });
}

void ShellClient::updateShowOnScreenEdge()
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
        Qt::Edges edges;
        for (int i = 0; i < screens()->count(); i++) {
            const auto &screenGeo = screens()->geometry(i);
            if (screenGeo.x() == geom.x()) {
                edges |= Qt::LeftEdge;
            }
            if (screenGeo.x() + screenGeo.width() == geom.x() + geom.width()) {
                edges |= Qt::RightEdge;
            }
            if (screenGeo.y() == geom.y()) {
                edges |= Qt::TopEdge;
            }
            if (screenGeo.y() + screenGeo.height() == geom.y() + geom.height()) {
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
        auto check = [this](Qt::Edges edges, Qt::Edge horiz, Qt::Edge vert) {
            if (edges.testFlag(horiz) && edges.testFlag(vert)) {
                if (geom.width() >= geom.height()) {
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

bool ShellClient::isInitialPositionSet() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->isPositionSet();
    }
    return false;
}

void ShellClient::installAppMenu(AppMenuInterface *menu)
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

void ShellClient::installPalette(ServerSideDecorationPaletteInterface *palette)
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

void ShellClient::updateColorScheme()
{
    if (m_paletteInterface) {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(m_paletteInterface->palette()));
    } else {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(QString()));
    }
}

void ShellClient::updateMaximizeMode(MaximizeMode maximizeMode)
{
    if (maximizeMode == m_maximizeMode) {
        return;
    }

    m_maximizeMode = maximizeMode;

    emit clientMaximizedStateChanged(this, m_maximizeMode);
    emit clientMaximizedStateChanged(this, m_maximizeMode & MaximizeHorizontal, m_maximizeMode & MaximizeVertical);
}

bool ShellClient::hasStrut() const
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

void ShellClient::updateIcon()
{
    const QString waylandIconName = QStringLiteral("wayland");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

bool ShellClient::isTransient() const
{
    return m_transient;
}

void ShellClient::setTransient()
{
    SurfaceInterface *s = nullptr;
    if (m_shellSurface) {
        s = m_shellSurface->transientFor().data();
    }
    if (m_xdgShellSurface) {
        if (auto transient = m_xdgShellSurface->transientFor().data()) {
            s = transient->surface();
        }
    }
    if (m_xdgShellPopup) {
        s = m_xdgShellPopup->transientFor().data();
    }
    if (!s) {
        s = waylandServer()->findForeignTransientForSurface(surface());
    }
    auto t = waylandServer()->findClient(s);
    if (t != transientFor()) {
        // remove from main client
        if (transientFor())
            transientFor()->removeTransient(this);
        setTransientFor(t);
        if (t) {
            t->addTransient(this);
        }
    }
    m_transient = (s != nullptr);
}

bool ShellClient::hasTransientPlacementHint() const
{
    return isTransient() && transientFor() != nullptr &&
            (m_shellSurface || m_xdgShellPopup);
}

QRect ShellClient::transientPlacement(const QRect &bounds) const
{
    QRect anchorRect;
    Qt::Edges anchorEdge;
    Qt::Edges gravity;
    QPoint offset;
    PositionerConstraints constraintAdjustments;
    QSize size = geometry().size();

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

    if (m_shellSurface) {
        anchorRect = QRect(m_shellSurface->transientOffset(), QSize(1,1));
        anchorEdge = Qt::TopEdge | Qt::LeftEdge;
        gravity = Qt::BottomEdge | Qt::RightEdge; //our single point represents the top left of the popup
        constraintAdjustments = (PositionerConstraint::SlideX | PositionerConstraint::SlideY);
    } else if (m_xdgShellPopup) {
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

QPoint ShellClient::popupOffset(const QRect &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity, const QSize popupSize) const
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

bool ShellClient::isWaitingForMoveResizeSync() const
{
    if (m_shellSurface) {
        return !m_pendingConfigureRequests.isEmpty();
    }
    return false;
}

void ShellClient::doResizeSync()
{
    requestGeometry(moveResizeGeometry());
}

QMatrix4x4 ShellClient::inputTransformation() const
{
    QMatrix4x4 m = Toplevel::inputTransformation();
    m.translate(-borderLeft(), -borderTop());
    return m;
}

void ShellClient::installServerSideDecoration(KWayland::Server::ServerSideDecorationInterface *deco)
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
                // maybe delay to next event cycle in case the ShellClient is getting destroyed, too
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

void ShellClient::installXdgDecoration(XdgDecorationInterface *deco)
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

bool ShellClient::shouldExposeToWindowManagement()
{
    if (m_internal) {
        return false;
    }
    if (isLockScreen()) {
        return false;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    if (m_shellSurface) {
        if (m_shellSurface->isTransient() && !m_shellSurface->acceptsKeyboardFocus()) {
            return false;
        }
    }
    return true;
}

KWayland::Server::XdgShellSurfaceInterface::States ShellClient::xdgSurfaceStates() const
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

void ShellClient::doMinimize()
{
    if (isMinimized()) {
        workspace()->clientHidden(this);
    } else {
        emit windowShown(this);
    }
    workspace()->updateMinimizedOfTransients(this);
}

bool ShellClient::setupCompositing()
{
    if (m_compositingSetup) {
        return true;
    }
    m_compositingSetup = Toplevel::setupCompositing();
    return m_compositingSetup;
}

void ShellClient::finishCompositing(ReleaseReason releaseReason)
{
    m_compositingSetup = false;
    Toplevel::finishCompositing(releaseReason);
}

void ShellClient::placeIn(QRect &area)
{
    Placement::self()->place(this, area);
    setGeometryRestore(geometry());
}

void ShellClient::showOnScreenEdge()
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

bool ShellClient::dockWantsInput() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Panel) {
            return m_plasmaShellSurface->panelTakesFocus();
        }
    }
    return false;
}

void ShellClient::killWindow()
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

bool ShellClient::hasPopupGrab() const
{
    return m_hasPopupGrab;
}

void ShellClient::popupDone()
{
    if (m_shellSurface) {
        m_shellSurface->popupDone();
    }
    if (m_xdgShellPopup) {
        m_xdgShellPopup->popupDone();
    }
}

void ShellClient::updateClientOutputs()
{
    QVector<OutputInterface*> clientOutputs;
    const auto outputs = waylandServer()->display()->outputs();
    for (OutputInterface* output: qAsConst(outputs)) {
        const QRect outputGeom(output->globalPosition(), output->pixelSize() / output->scale());
        if (geometry().intersects(outputGeom)) {
            clientOutputs << output;
        }
    }
    surface()->setOutputs(clientOutputs);
}

void ShellClient::updateWindowMargins()
{
    QRect windowGeometry;
    QSize clientSize = m_clientSize;

    if (m_xdgShellSurface) {
        windowGeometry = m_xdgShellSurface->windowGeometry();
    } else if (m_xdgShellPopup) {
        windowGeometry = m_xdgShellPopup->windowGeometry();
        if (!clientSize.isValid()) {
            clientSize = m_xdgShellPopup->initialSize();
        }
    } else {
        return;
    }

    if (windowGeometry.isEmpty() ||
        windowGeometry.width() > clientSize.width() ||
        windowGeometry.height() > clientSize.height()) {
        m_windowMargins = QMargins();
    } else {
        m_windowMargins = QMargins(windowGeometry.left(),
                                    windowGeometry.top(),
                                    clientSize.width() - (windowGeometry.right() + 1),
                                    clientSize.height() - (windowGeometry.bottom() + 1));
    }
}

bool ShellClient::isPopupWindow() const
{
    if (Toplevel::isPopupWindow()) {
        return true;
    }
    if (m_shellSurface != nullptr) {
        return m_shellSurface->isPopup();
    }
    if (m_xdgShellPopup != nullptr) {
        return true;
    }
    return false;
}

QWindow *ShellClient::internalWindow() const
{
    return nullptr;
}

}
