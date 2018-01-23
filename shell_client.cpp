/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "workspace.h"
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

#include <KDesktopFile>

#include <QOpenGLFramebufferObject>
#include <QWindow>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

using namespace KWayland::Server;

static const QByteArray s_skipClosePropertyName = QByteArrayLiteral("KWIN_SKIP_CLOSE_ANIMATION");

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
    init();
}

ShellClient::ShellClient(XdgShellPopupInterface *surface)
    : AbstractClient()
    , m_shellSurface(nullptr)
    , m_xdgShellSurface(nullptr)
    , m_xdgShellPopup(surface)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    init();
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

    setResourceClass(shellSurface->windowClass());
    setDesktopFileName(shellSurface->windowClass());
    connect(shellSurface, &T::windowClassChanged, this,
        [this] (const QByteArray &windowClass) {
            setResourceClass(windowClass);
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
}

void ShellClient::init()
{
    connect(this, &ShellClient::desktopFileNameChanged, this, &ShellClient::updateIcon);
    findInternalWindow();
    createWindowId();
    setupCompositing();
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
    if (m_internalWindow) {
        updateInternalWindowGeometry();
        updateDecoration(true);
    } else {
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
            m_xdgShellSurface->configure(xdgSurfaceStates());
        };
        configure();
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

        QRect position = QRect(m_xdgShellPopup->transientOffset(), m_xdgShellPopup->initialSize());
        m_xdgShellPopup->configure(position);

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
    // check whether we have a ServerSideDecoration
    if (ServerSideDecorationInterface *deco = ServerSideDecorationInterface::get(s)) {
        installServerSideDecoration(deco);
    }

    AbstractClient::updateColorScheme(QString());

    if (!m_internal) {
        discardTemporaryRules();
        applyWindowRules(); // Just in case
        RuleBook::self()->discardUsed(this, false);   // Remove ApplyNow rules
        updateWindowRules(Rules::All); // Was blocked while !isManaged()
    }
}

void ShellClient::destroyClient()
{
    m_closing = true;
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
    // TODO: implement
    Q_UNUSED(stream)
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
        QPoint position = geom.topLeft();
        if (m_positionAfterResize.isValid()) {
            addLayerRepaint(geometry());
            position = m_positionAfterResize.point();
            m_positionAfterResize.clear();
        }
        doSetGeometry(QRect(position, m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
    markAsMapped();
    setDepth((s->buffer()->hasAlphaChannel() && !isDesktop()) ? 32 : 24);
    repaints_region += damage.translated(clientPos());
    Toplevel::addDamage(damage);
}

void ShellClient::setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo)
{
    if (fbo.isNull()) {
        unmap();
        return;
    }

    //Kwin currently scales internal windows to 1, so this is currently always correct
    //when that changes, this needs adjusting
    m_clientSize = fbo->size();
    markAsMapped();
    doSetGeometry(QRect(geom.topLeft(), m_clientSize));
    Toplevel::setInternalFramebufferObject(fbo);
    Toplevel::addDamage(QRegion(0, 0, width(), height()));
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
    // TODO: better merge with Client's implementation
    if (QSize(w, h) == geom.size() && !m_positionAfterResize.isValid()) {
        // size didn't change, update directly
        doSetGeometry(QRect(x, y, w, h));
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
    syncGeometryToInternalWindow();
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

void ShellClient::doMove(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    syncGeometryToInternalWindow();
}

void ShellClient::syncGeometryToInternalWindow()
{
    if (!m_internalWindow) {
        return;
    }
    const QRect windowRect = QRect(geom.topLeft() + QPoint(borderLeft(), borderTop()),
                                    geom.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
    if (m_internalWindow->geometry() != windowRect) {
        // delay to end of cycle to prevent freeze, see BUG 384441
        QTimer::singleShot(0, m_internalWindow, std::bind(static_cast<void (QWindow::*)(const QRect&)>(&QWindow::setGeometry), m_internalWindow, windowRect));
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
    } else if (m_qtExtendedSurface && isCloseable()) {
        m_qtExtendedSurface->close();
    } else if (m_internalWindow) {
        m_internalWindow->hide();
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
    if (m_internal) {
        return true;
    }
    return m_qtExtendedSurface ? true : false;
}

bool ShellClient::isFullScreen() const
{
    return m_fullScreen;
}

bool ShellClient::isMaximizable() const
{
    if (m_internal) {
        return false;
    }
    return true;
}

bool ShellClient::isMinimizable() const
{
    if (m_internal) {
        return false;
    }
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

    MaximizeMode oldMode = m_maximizeMode;
    StackingUpdatesBlocker blocker(workspace());
    RequestGeometryBlocker geometryBlocker(this);
    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if (!adjust) {
        if (vertical)
            m_maximizeMode = MaximizeMode(m_maximizeMode ^ MaximizeVertical);
        if (horizontal)
            m_maximizeMode = MaximizeMode(m_maximizeMode ^ MaximizeHorizontal);
    }
    // TODO: add more checks as in Client

    // call into decoration update borders
    if (isDecorated() && decoration()->client() && !(options->borderlessMaximizedWindows() && m_maximizeMode == KWin::MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = decoration()->client().data();
        if ((m_maximizeMode & MaximizeVertical) != (oldMode & MaximizeVertical)) {
            emit c->maximizedVerticallyChanged(m_maximizeMode & MaximizeVertical);
        }
        if ((m_maximizeMode & MaximizeHorizontal) != (oldMode & MaximizeHorizontal)) {
            emit c->maximizedHorizontallyChanged(m_maximizeMode & MaximizeHorizontal);
        }
        if ((m_maximizeMode == MaximizeFull) != (oldMode == MaximizeFull)) {
            emit c->maximizedChanged(m_maximizeMode & MaximizeFull);
        }
        changeMaximizeRecursion = false;
    }

    if (options->borderlessMaximizedWindows()) {
        // triggers a maximize change.
        // The next setNoBorder interation will exit since there's no change but the first recursion pullutes the restore geometry
        changeMaximizeRecursion = true;
        setNoBorder(rules()->checkNoBorder(m_maximizeMode == MaximizeFull));
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
        } else if ((oldMode == MaximizeVertical && m_maximizeMode == MaximizeRestore) ||
                  (oldMode == MaximizeFull && m_maximizeMode == MaximizeHorizontal)) {
            // Modifying geometry of a tiled window
            updateQuickTileMode(QuickTileFlag::None); // Exit quick tile mode without restoring geometry
        }
    }

    // TODO: check rules
    if (m_maximizeMode == MaximizeFull) {
        m_geomMaximizeRestore = geometry();
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
        if (m_maximizeMode == MaximizeRestore) {
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

bool ShellClient::noBorder() const
{
    if (isInternal()) {
        return m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
    }
    if (m_serverDecoration) {
        if (m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
            return m_userNoBorder || isFullScreen();
        }
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
    if (was_fs)
        workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event
    else
        m_geomFsRestore = geometry();
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
        if (!m_geomFsRestore.isNull()) {
            int currentScreen = screen();
            setGeometry(QRect(m_geomFsRestore.topLeft(), adjustedSize(m_geomFsRestore.size())));
            if( currentScreen != screen())
                workspace()->sendClientToScreen( this, currentScreen );
        } else {
            // does this ever happen?
            setGeometry(workspace()->clientArea(MaximizeArea, this));
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

    bool breakShowingDesktop = !keepAbove() && !isOnScreenDisplay();
    if (breakShowingDesktop) {
        // check that it doesn't belong to the desktop
        const auto &clients = waylandServer()->clients();
        for (auto c: clients) {
            if (!belongsToSameApplication(c, SameApplicationChecks())) {
                continue;
            }
            if (c->isDesktop()) {
                breakShowingDesktop = false;
                break;
            }
        }
    }
    if (breakShowingDesktop)
        workspace()->setShowingDesktop(false);
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
    if (m_internal) {
        return !m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
    }
    return false;
}

bool ShellClient::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus());
}

bool ShellClient::acceptsFocus() const
{
    if (isInternal()) {
        return false;
    }
    if (waylandServer()->inputMethodConnection() == surface()->client()) {
        return false;
    }
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::OnScreenDisplay ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::ToolTip ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Notification) {
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
    if (m_internalWindow) {
        m_windowId = m_internalWindow->winId();
    } else {
        m_windowId = waylandServer()->createWindowId(surface());
    }
}

void ShellClient::findInternalWindow()
{
    if (surface()->client() != waylandServer()->internalConnection()) {
        return;
    }
    const QWindowList windows = kwinApp()->topLevelWindows();
    for (QWindow *w: windows) {
        auto s = KWayland::Client::Surface::fromWindow(w);
        if (!s) {
            continue;
        }
        if (s->id() != surface()->id()) {
            continue;
        }
        m_internalWindow = w;
        m_internalWindowFlags = m_internalWindow->flags();
        connect(m_internalWindow, &QWindow::xChanged, this, &ShellClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::yChanged, this, &ShellClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::destroyed, this, [this] { m_internalWindow = nullptr; });
        connect(m_internalWindow, &QWindow::opacityChanged, this, &ShellClient::setOpacity);

        // Try reading the window type from the QWindow. PlasmaCore.Dialog provides a dynamic type property
        // let's check whether it exists, if it does it's our window type
        const QVariant windowType = m_internalWindow->property("type");
        if (!windowType.isNull()) {
            m_windowType = static_cast<NET::WindowType>(windowType.toInt());
        }
        setOpacity(m_internalWindow->opacity());

        // skip close animation support
        setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        m_internalWindow->installEventFilter(this);
        return;
    }
}

void ShellClient::updateInternalWindowGeometry()
{
    if (!m_internalWindow) {
        return;
    }
    doSetGeometry(QRect(m_internalWindow->geometry().topLeft() - QPoint(borderLeft(), borderTop()),
                        m_internalWindow->geometry().size() + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
}

pid_t ShellClient::pid() const
{
    return surface()->client()->processId();
}

bool ShellClient::isInternal() const
{
    return m_internal;
}

bool ShellClient::isLockScreen() const
{
    if (m_internalWindow) {
        return m_internalWindow->property("org_kde_ksld_emergency").toBool();
    }
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool ShellClient::isInputMethod() const
{
    if (m_internal && m_internalWindow) {
        return m_internalWindow->property("__kwin_input_method").toBool();
    }
    return surface()->client() == waylandServer()->inputMethodConnection();
}

void ShellClient::requestGeometry(const QRect &rect)
{
    if (m_requestGeometryBlockCounter != 0) {
        m_blockedRequestGeometry = rect;
        return;
    }
    m_positionAfterResize.setPoint(rect.topLeft());
    const QSize size = rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    if (m_shellSurface) {
        m_shellSurface->requestSize(size);
    }
    if (m_xdgShellSurface) {
        m_xdgShellSurface->configure(xdgSurfaceStates(), size);
    }
    if (m_xdgShellPopup) {
        auto parent = transientFor();
        if (parent) {
            const QPoint globalClientContentPos = parent->geometry().topLeft() + parent->clientPos();
            const QPoint relativeOffset = rect.topLeft() -globalClientContentPos;
            m_xdgShellPopup->configure(QRect(relativeOffset, rect.size()));
        }
    }

    m_blockedRequestGeometry = QRect();
    if (m_internal) {
        m_internalWindow->setGeometry(QRect(rect.topLeft() + QPoint(borderLeft(), borderTop()), rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
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
    if (m_internal) {
        m_internalWindow->setGeometry(QRect(pos() + QPoint(borderLeft(), borderTop()), QSize(w, h) - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
}

void ShellClient::unmap()
{
    m_unmapped = true;
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
        doSetGeometry(QRect(surface->position(), m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
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
        case PlasmaShellSurfaceInterface::Role::Normal:
        default:
            type = NET::Normal;
            break;
        }
        if (type != m_windowType) {
            m_windowType = type;
            if (m_windowType == NET::Desktop || type == NET::Dock || type == NET::OnScreenDisplay || type == NET::Notification || type == NET::Tooltip) {
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

void ShellClient::installQtExtendedSurface(QtExtendedSurfaceInterface *surface)
{
    m_qtExtendedSurface = surface;

    connect(m_qtExtendedSurface.data(), &QtExtendedSurfaceInterface::raiseRequested, this, [this]() {
        workspace()->raiseClientRequest(this);
    });
    connect(m_qtExtendedSurface.data(), &QtExtendedSurfaceInterface::lowerRequested, this, [this]() {
        workspace()->lowerClientRequest(this);
    });
    m_qtExtendedSurface->installEventFilter(this);
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


bool ShellClient::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_internalWindow && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (pe->propertyName() == s_skipClosePropertyName) {
            setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        }
    }
    return false;
}

void ShellClient::updateColorScheme()
{
    if (m_paletteInterface) {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(m_paletteInterface->palette()));
    } else {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(QString()));
    }
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
    return isTransient() && transientFor() != nullptr;
}

QPoint ShellClient::transientPlacementHint() const
{
    if (m_shellSurface) {
        return m_shellSurface->transientOffset();
    }
    if (m_xdgShellPopup) {
        return m_xdgShellPopup->transientOffset();
    }
    return QPoint();
}

bool ShellClient::isWaitingForMoveResizeSync() const
{
    return m_positionAfterResize.isValid();
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

bool ShellClient::shouldExposeToWindowManagement()
{
    if (isInternal()) {
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
    if (maximizeMode() == MaximizeMode::MaximizeFull) {
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
    if (isInternal()) {
        return;
    }
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

}
