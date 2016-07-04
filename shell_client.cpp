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
#include "decorations/decorationbridge.h"
#include "decorations/decoratedclient.h"
#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <KWayland/Client/surface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/plasmashell_interface.h>
#include <KWayland/Server/shadow_interface.h>
#include <KWayland/Server/server_decoration_interface.h>
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
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    init();
}

ShellClient::~ShellClient() = default;

void ShellClient::init()
{
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
        m_clientSize = s->buffer()->size();
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
    if (waylandServer()->inputMethodConnection() == s->client()) {
        m_windowType = NET::OnScreenDisplay;
    }

    connect(s, &SurfaceInterface::sizeChanged, this,
        [this] {
            m_clientSize = surface()->buffer()->size();
            doSetGeometry(QRect(geom.topLeft(), m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
            discardWindowPixmap();
        }
    );
    connect(s, &SurfaceInterface::unmapped, this, &ShellClient::unmap);
    connect(s, &SurfaceInterface::destroyed, this, &ShellClient::destroyClient);
    if (m_shellSurface) {
        m_caption = m_shellSurface->title();
        connect(m_shellSurface, &ShellSurfaceInterface::destroyed, this, &ShellClient::destroyClient);
        connect(m_shellSurface, &ShellSurfaceInterface::titleChanged, this,
            [this] {
                m_caption = m_shellSurface->title();
                emit captionChanged();
            }
        );

        connect(m_shellSurface, &ShellSurfaceInterface::fullscreenChanged, this, &ShellClient::clientFullScreenChanged);
        connect(m_shellSurface, &ShellSurfaceInterface::maximizedChanged, this,
            [this] (bool maximized) {
                maximize(maximized ? MaximizeFull : MaximizeRestore);
            }
        );
        connect(m_shellSurface, &ShellSurfaceInterface::windowClassChanged, this, &ShellClient::updateIcon);

        setResourceClass(m_shellSurface->windowClass());
        connect(m_shellSurface, &ShellSurfaceInterface::windowClassChanged, this,
            [this] {
                setResourceClass(m_shellSurface->windowClass());
            }
        );
        connect(m_shellSurface, &ShellSurfaceInterface::transientForChanged, this, &ShellClient::setTransient);
        connect(m_shellSurface, &ShellSurfaceInterface::moveRequested, this,
            [this] {
                // TODO: check the seat and serial
                performMouseCommand(Options::MouseMove, Cursor::pos());
            }
        );
        connect(m_shellSurface, &ShellSurfaceInterface::resizeRequested, this,
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
    updateIcon();

    // setup shadow integration
    getShadow();
    connect(s, &SurfaceInterface::shadowChanged, this, &Toplevel::getShadow);

    setTransient();
    // check whether we have a ServerSideDecoration
    if (ServerSideDecorationInterface *deco = ServerSideDecorationInterface::get(s)) {
        installServerSideDecoration(deco);
    }

    updateColorScheme(QString());
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
    if (s->buffer()->size().isValid()) {
        m_clientSize = s->buffer()->size();
        QPoint position = geom.topLeft();
        if (m_positionAfterResize.isValid()) {
            addLayerRepaint(geometry());
            position = m_positionAfterResize.point();
            m_positionAfterResize.clear();
        }
        doSetGeometry(QRect(position, m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
    markAsMapped();
    setDepth(s->buffer()->hasAlphaChannel() ? 32 : 24);
    repaints_region += damage.translated(clientPos());
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
    if (!ready_for_painting) {
        setReadyForPainting();
    } else {
        addRepaintFull();
        emit windowShown(this);
    }
    if (shouldExposeToWindowManagement()) {
        setupWindowManagementInterface();
    }
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
    triggerDecorationRepaint();
    if (hasStrut()) {
        workspace()->updateClientArea();
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
    return m_caption;
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
    if (!windowManagementInterface()) {
        // window management interface is only available if the surface is mapped
        return QRect();
    }

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
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    return true;
}

bool ShellClient::isMovableAcrossScreens() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    return true;
}

bool ShellClient::isResizable() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
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

static bool changeMaximizeRecursion = false;
void ShellClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    if (changeMaximizeRecursion) {
        return;
    }
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
}

MaximizeMode ShellClient::maximizeMode() const
{
    return m_maximizeMode;
}

bool ShellClient::noBorder() const
{
    if (isInternal()) {
        return true;
    }
    if (m_serverDecoration) {
        if (m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
            return m_userNoBorder;
        }
    }
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
    if (m_serverDecoration && m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
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
    if (m_shellSurface) {
        if (m_shellSurface->isPopup()) {
            return false;
        }
        // if the window is not visible it doesn't get input
        return m_shellSurface->acceptsKeyboardFocus() && isShown(true);
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
    return m_internal;
}

bool ShellClient::isLockScreen() const
{
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
    if (m_shellSurface) {
        m_shellSurface->requestSize(rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
    }
    m_blockedRequestGeometry = QRect();
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
    if (m_shellSurface) {
        m_shellSurface->requestSize(QSize(w, h));
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
    QString desktopFile;
    if (m_shellSurface) {
        desktopFile = QString::fromUtf8(m_shellSurface->windowClass());
    }
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
    SurfaceInterface *s = nullptr;
    if (m_shellSurface) {
        s = m_shellSurface->transientFor().data();
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
            // always acknowledge the requested mode
            m_serverDecoration->setMode(mode);
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
    if (m_shellSurface) {
        if (m_shellSurface->isTransient() && !m_shellSurface->acceptsKeyboardFocus()) {
            return false;
        }
    }
    return true;
}

}
