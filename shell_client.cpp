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
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "virtualdesktops.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/plasmashell_interface.h>
#include <KWayland/Server/shadow_interface.h>
#include <KWayland/Server/qtsurfaceextension_interface.h>
#include <KWayland/Server/plasmawindowmanagement_interface.h>

#include <KDesktopFile>

#include <QOpenGLFramebufferObject>
#include <QWindow>

using namespace KWayland::Server;

namespace KWin
{

ShellClient::ShellClient(ShellSurfaceInterface *surface)
    : AbstractClient()
    , m_shellSurface(surface)
{
    setSurface(surface->surface());
    findInternalWindow();
    createWindowId();
    setupCompositing();
    if (surface->surface()->buffer()) {
        setReadyForPainting();
        setupWindowManagementInterface();
        m_unmapped = false;
        m_clientSize = surface->surface()->buffer()->size();
    } else {
        ready_for_painting = false;
    }
    if (m_internalWindow) {
        updateInternalWindowGeometry();
        setOnAllDesktops(true);
    } else {
        doSetGeometry(QRect(QPoint(0, 0), m_clientSize));
        setDesktop(VirtualDesktopManager::self()->current());
    }
    if (waylandServer()->inputMethodConnection() == m_shellSurface->client()) {
        m_windowType = NET::OnScreenDisplay;
    }

    connect(surface->surface(), &SurfaceInterface::sizeChanged, this,
        [this] {
            m_clientSize = m_shellSurface->surface()->buffer()->size();
            doSetGeometry(QRect(geom.topLeft(), m_clientSize));
            discardWindowPixmap();
        }
    );
    connect(surface, &ShellSurfaceInterface::destroyed, this, &ShellClient::destroyClient);
    connect(surface->surface(), &SurfaceInterface::unmapped, this, &ShellClient::unmap);
    connect(surface, &ShellSurfaceInterface::titleChanged, this, &ShellClient::captionChanged);

    connect(surface, &ShellSurfaceInterface::fullscreenChanged, this, &ShellClient::clientFullScreenChanged);
    connect(surface, &ShellSurfaceInterface::maximizedChanged, this,
        [this] (bool maximized) {
            maximize(maximized ? MaximizeFull : MaximizeRestore);
        }
    );
    connect(surface, &ShellSurfaceInterface::windowClassChanged, this, &ShellClient::updateIcon);
    updateIcon();

    // setup shadow integration
    getShadow();
    connect(surface->surface(), &SurfaceInterface::shadowChanged, this, &Toplevel::getShadow);

    setResourceClass(surface->windowClass());
    connect(surface, &ShellSurfaceInterface::windowClassChanged, this,
        [this] {
            setResourceClass(m_shellSurface->windowClass());
        }
    );

    setTransient();
    connect(surface, &ShellSurfaceInterface::transientForChanged, this, &ShellClient::setTransient);
    connect(surface, &ShellSurfaceInterface::moveRequested, this,
        [this] {
            // TODO: check the seat and serial
            performMouseCommand(Options::MouseMove, Cursor::pos());
        }
    );
    connect(surface, &ShellSurfaceInterface::resizeRequested, this,
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
}

ShellClient::~ShellClient() = default;

void ShellClient::destroyClient()
{
    m_closing = true;
    Deleted *del = nullptr;
    if (workspace()) {
        del = Deleted::create(this);
    }
    emit windowClosed(this, del);

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
    // TODO: connect for changes
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

bool ShellClient::shouldUnredirect() const
{
    // TODO: unredirect for fullscreen
    return false;
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
    return 1.0;
}

void ShellClient::setOpacity(double opacity)
{
    Q_UNUSED(opacity)
}

void ShellClient::addDamage(const QRegion &damage)
{
    if (m_shellSurface->surface()->buffer()->size().isValid()) {
        m_clientSize = m_shellSurface->surface()->buffer()->size();
        QPoint position = geom.topLeft();
        if (m_positionAfterResize.isValid()) {
            addLayerRepaint(geometry());
            position = m_positionAfterResize.point();
            m_positionAfterResize.clear();
        }
        doSetGeometry(QRect(position, m_clientSize));
    }
    markAsMapped();
    setDepth(m_shellSurface->surface()->buffer()->hasAlphaChannel() ? 32 : 24);
    Toplevel::addDamage(damage);
}

void ShellClient::setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo)
{
    if (fbo.isNull()) {
        unmap();
        return;
    }
    markAsMapped();
    m_clientSize = fbo->size();
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
    setReadyForPainting();
    setupWindowManagementInterface();
}

void ShellClient::setGeometry(int x, int y, int w, int h, ForceGeometry_t force)
{
    Q_UNUSED(force)
    // TODO: better merge with Client's implementation
    if (QSize(w, h) == geom.size()) {
        // size didn't change, update directly
        doSetGeometry(QRect(x, y, w, h));
    } else {
        // size did change, Client needs to provide a new buffer
        requestGeometry(QRect(x, y, w, h));
    }
}

void ShellClient::doSetGeometry(const QRect &rect)
{
    if (geom == rect) {
        return;
    }
    if (!m_unmapped) {
        addWorkspaceRepaint(visibleRect());
    }
    const QRect old = geom;
    geom = rect;

    if (m_unmapped && m_geomMaximizeRestore.isEmpty() && !geom.isEmpty()) {
        // use first valid geometry as restore geometry
        // TODO: needs to interact with placing. The first valid geometry should be the placed one
        m_geomMaximizeRestore = geom;
    }

    if (!m_unmapped) {
        addWorkspaceRepaint(visibleRect());
    }
    emit geometryShapeChanged(this, old);
}

QByteArray ShellClient::windowRole() const
{
    return QByteArray();
}

bool ShellClient::belongsToSameApplication(const AbstractClient *other, bool active_hack) const
{
    Q_UNUSED(active_hack)
    if (auto s = other->surface()) {
        return s->client() == surface()->client();
    }
    return false;
}

void ShellClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)
}

QString ShellClient::caption(bool full, bool stripped) const
{
    Q_UNUSED(full)
    Q_UNUSED(stripped)
    return m_shellSurface->title();
}

void ShellClient::closeWindow()
{
    if (m_qtExtendedSurface && isCloseable()) {
        m_qtExtendedSurface->close();
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
    return m_qtExtendedSurface ? true : false;
}

bool ShellClient::isFullScreenable() const
{
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

QRect ShellClient::iconGeometry() const
{
    int minDistance = INT_MAX;
    ShellClient *candidatePanel = nullptr;
    QRect candidateGeom;

    for (auto i = windowManagementInterface()->minimizedGeometries().constBegin(), end = windowManagementInterface()->minimizedGeometries().constEnd(); i != end; ++i) {
        ShellClient *client = waylandServer()->findClient(i.key());
        if (!client) {
            continue;
        }
        const int distance = QPoint(client->pos() - pos()).manhattanLength();
        if (distance < minDistance) {
            minDistance = distance;
            candidatePanel = client;
            candidateGeom = i.value();
        }
    }
    if (!candidatePanel) {
        return QRect();
    }
    return candidateGeom.translated(candidatePanel->pos());
}

bool ShellClient::isMovable() const
{
    return true;
}

bool ShellClient::isMovableAcrossScreens() const
{
    return true;
}

bool ShellClient::isResizable() const
{
    return true;
}

bool ShellClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !m_closing && !m_unmapped;
}

void ShellClient::hideClient(bool hide)
{
    Q_UNUSED(hide)
}

void ShellClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    StackingUpdatesBlocker blocker(workspace());
    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if (!adjust) {
        if (vertical)
            m_maximizeMode = MaximizeMode(m_maximizeMode ^ MaximizeVertical);
        if (horizontal)
            m_maximizeMode = MaximizeMode(m_maximizeMode ^ MaximizeHorizontal);
    }

    // TODO: check rules
    if (m_maximizeMode == MaximizeFull) {
        m_geomMaximizeRestore = geometry();
        requestGeometry(workspace()->clientArea(MaximizeArea, this));
        workspace()->raiseClient(this);
    } else {
        if (m_geomMaximizeRestore.isValid()) {
            requestGeometry(m_geomMaximizeRestore);
        } else {
            requestGeometry(workspace()->clientArea(PlacementArea, this));
        }
    }
    // TODO: add more checks as in Client
}

MaximizeMode ShellClient::maximizeMode() const
{
    return m_maximizeMode;
}

bool ShellClient::noBorder() const
{
    return true;
}

const WindowRules *ShellClient::rules() const
{
    static WindowRules s_rules;
    return &s_rules;
}

void ShellClient::setFullScreen(bool set, bool user)
{
    Q_UNUSED(set)
    Q_UNUSED(user)
}

void ShellClient::setNoBorder(bool set)
{
    Q_UNUSED(set)
}

void ShellClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)
}

void ShellClient::setShortcut(const QString &cut)
{
    Q_UNUSED(cut)
}

const QKeySequence &ShellClient::shortcut() const
{
    static QKeySequence seq;
    return seq;
}

void ShellClient::takeFocus()
{
    if (rules()->checkAcceptFocus(wantsInput())) {
        setActive(true);
    }

    bool breakShowingDesktop = !keepAbove() && !isOnScreenDisplay();
    if (breakShowingDesktop) {
        // check that it doesn't belong to the desktop
        const auto &clients = waylandServer()->clients();
        for (auto c: clients) {
            if (!belongsToSameApplication(c, false)) {
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
    StackingUpdatesBlocker blocker(workspace());
    workspace()->focusToNull();
}

void ShellClient::updateWindowRules(Rules::Types selection)
{
    Q_UNUSED(selection)
}

bool ShellClient::userCanSetFullScreen() const
{
    return false;
}

bool ShellClient::userCanSetNoBorder() const
{
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
    if (waylandServer()->inputMethodConnection() == m_shellSurface->client()) {
        return false;
    }
    if (m_shellSurface->isPopup()) {
        return false;
    }
    // if the window is not visible it doesn't get input
    return m_shellSurface->acceptsKeyboardFocus() && isShown(true);
}

void ShellClient::createWindowId()
{
    if (m_internalWindow) {
        m_windowId = m_internalWindow->winId();
    } else {
        m_windowId = waylandServer()->createWindowId(m_shellSurface->surface());
    }
}

void ShellClient::findInternalWindow()
{
    if (m_shellSurface->client() != waylandServer()->internalConnection()) {
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
        connect(m_internalWindow, &QWindow::xChanged, this, &ShellClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::yChanged, this, &ShellClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::destroyed, this, [this] { m_internalWindow = nullptr; });

        // Try reading the window type from the QWindow. PlasmaCore.Dialog provides a dynamic type property
        // let's check whether it exists, if it does it's our window type
        const QVariant windowType = m_internalWindow->property("type");
        if (!windowType.isNull()) {
            m_windowType = static_cast<NET::WindowType>(windowType.toInt());
        }
        return;
    }
}

void ShellClient::updateInternalWindowGeometry()
{
    if (!m_internalWindow) {
        return;
    }
    doSetGeometry(m_internalWindow->geometry());
}

bool ShellClient::isInternal() const
{
    return m_shellSurface->client() == waylandServer()->internalConnection();
}

bool ShellClient::isLockScreen() const
{
    return m_shellSurface->client() == waylandServer()->screenLockerClientConnection();
}

bool ShellClient::isInputMethod() const
{
    return m_shellSurface->client() == waylandServer()->inputMethodConnection();
}

xcb_window_t ShellClient::window() const
{
    return windowId();
}

void ShellClient::requestGeometry(const QRect &rect)
{
    m_positionAfterResize.setPoint(rect.topLeft());
    m_shellSurface->requestSize(rect.size());
}

void ShellClient::clientFullScreenChanged(bool fullScreen)
{
    StackingUpdatesBlocker blocker(workspace());
    workspace()->updateClientLayer(this);   // active fullscreens get different layer

    if (fullScreen) {
        m_geomFsRestore = geometry();
        requestGeometry(workspace()->clientArea(FullScreenArea, this));
        workspace()->raiseClient(this);
    } else {
        if (m_geomFsRestore.isValid()) {
            requestGeometry(m_geomFsRestore);
        } else {
            requestGeometry(workspace()->clientArea(MaximizeArea, this));
        }
    }
    if (m_fullScreen != fullScreen) {
        m_fullScreen = fullScreen;
        emit fullScreenChanged();
    }
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
    m_shellSurface->requestSize(QSize(w, h));
}

void ShellClient::unmap()
{
    m_unmapped = true;
    ready_for_painting = false;
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
        doSetGeometry(QRect(surface->position(), m_clientSize));
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
        case PlasmaShellSurfaceInterface::Role::Normal:
        default:
            type = NET::Normal;
            break;
        }
        if (type != m_windowType) {
            m_windowType = type;
            workspace()->updateClientArea();
        }
    };
    connect(surface, &PlasmaShellSurfaceInterface::positionChanged, this, updatePosition);
    connect(surface, &PlasmaShellSurfaceInterface::roleChanged, this, updateRole);
    connect(surface, &PlasmaShellSurfaceInterface::panelBehaviorChanged, this,
        [] {
            workspace()->updateClientArea();
        }
    );
    updatePosition();
    updateRole();

    setSkipTaskbar(surface->skipTaskbar());
    connect(surface, &PlasmaShellSurfaceInterface::skipTaskbarChanged, this, [this] {
        setSkipTaskbar(m_plasmaShellSurface->skipTaskbar());
    });
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
    return m_plasmaShellSurface->panelBehavior() != PlasmaShellSurfaceInterface::PanelBehavior::WindowsGoBelow;
}

void ShellClient::updateIcon()
{
    QString desktopFile = QString::fromUtf8(m_shellSurface->windowClass());
    if (desktopFile.isEmpty()) {
        setIcon(QIcon());
    }
    if (!desktopFile.endsWith(QLatin1String(".desktop"))) {
        desktopFile.append(QLatin1String(".desktop"));
    }
    KDesktopFile df(desktopFile);
    setIcon(QIcon::fromTheme(df.readIcon()));
}

bool ShellClient::isTransient() const
{
    return m_transient;
}

void ShellClient::setTransient()
{
    const auto s = m_shellSurface->transientFor();
    auto t = waylandServer()->findClient(s.data());
    if (t != transientFor()) {
        // remove from main client
        if (transientFor())
            transientFor()->removeTransient(this);
        setTransientFor(t);
        if (t) {
            t->addTransient(this);
        }
    }
    m_transient = !s.isNull();
}

bool ShellClient::hasTransientPlacementHint() const
{
    return isTransient() && transientFor() != nullptr;
}

QPoint ShellClient::transientPlacementHint() const
{
    return m_shellSurface->transientOffset();
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

}
