/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "workspace_wrapper.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "outline.h"
#include "tiles/tilemanager.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "x11window.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif

namespace KWin
{

WorkspaceWrapper::WorkspaceWrapper(QObject *parent)
    : QObject(parent)
{
    KWin::Workspace *ws = KWin::Workspace::self();
    KWin::VirtualDesktopManager *vds = KWin::VirtualDesktopManager::self();
    connect(ws, &Workspace::desktopPresenceChanged, this, &WorkspaceWrapper::desktopPresenceChanged);
    connect(ws, &Workspace::currentDesktopChanged, this, &WorkspaceWrapper::currentDesktopChanged);
    connect(ws, &Workspace::windowAdded, this, &WorkspaceWrapper::clientAdded);
    connect(ws, &Workspace::windowAdded, this, &WorkspaceWrapper::setupClientConnections);
    connect(ws, &Workspace::windowRemoved, this, &WorkspaceWrapper::clientRemoved);
    connect(ws, &Workspace::windowActivated, this, &WorkspaceWrapper::clientActivated);
    connect(vds, &VirtualDesktopManager::countChanged, this, &WorkspaceWrapper::numberDesktopsChanged);
    connect(vds, &VirtualDesktopManager::layoutChanged, this, &WorkspaceWrapper::desktopLayoutChanged);
    connect(vds, &VirtualDesktopManager::currentChanged, this, &WorkspaceWrapper::currentVirtualDesktopChanged);
    connect(ws, &Workspace::windowDemandsAttentionChanged, this, &WorkspaceWrapper::clientDemandsAttentionChanged);
#if KWIN_BUILD_ACTIVITIES
    if (KWin::Activities *activities = ws->activities()) {
        connect(activities, &Activities::currentChanged, this, &WorkspaceWrapper::currentActivityChanged);
        connect(activities, &Activities::added, this, &WorkspaceWrapper::activitiesChanged);
        connect(activities, &Activities::added, this, &WorkspaceWrapper::activityAdded);
        connect(activities, &Activities::removed, this, &WorkspaceWrapper::activitiesChanged);
        connect(activities, &Activities::removed, this, &WorkspaceWrapper::activityRemoved);
    }
#endif
    connect(ws, &Workspace::geometryChanged, this, &WorkspaceWrapper::virtualScreenSizeChanged);
    connect(ws, &Workspace::geometryChanged, this, &WorkspaceWrapper::virtualScreenGeometryChanged);
    connect(ws, &Workspace::outputAdded, this, [this]() {
        Q_EMIT numberScreensChanged(numScreens());
    });
    connect(ws, &Workspace::outputRemoved, this, [this]() {
        Q_EMIT numberScreensChanged(numScreens());
    });
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(QApplication::desktop(), &QDesktopWidget::resized, this, &WorkspaceWrapper::screenResized);
#endif
    connect(Cursors::self()->mouse(), &Cursor::posChanged, this, &WorkspaceWrapper::cursorPosChanged);

    const QList<Window *> clients = ws->allClientList();
    for (Window *client : clients) {
        setupClientConnections(client);
    }
}

int WorkspaceWrapper::currentDesktop() const
{
    return VirtualDesktopManager::self()->current();
}

VirtualDesktop *WorkspaceWrapper::currentVirtualDesktop() const
{
    return VirtualDesktopManager::self()->currentDesktop();
}

int WorkspaceWrapper::numberOfDesktops() const
{
    return VirtualDesktopManager::self()->count();
}

void WorkspaceWrapper::setCurrentDesktop(int desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

void WorkspaceWrapper::setCurrentVirtualDesktop(VirtualDesktop *desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

void WorkspaceWrapper::setNumberOfDesktops(int count)
{
    VirtualDesktopManager::self()->setCount(count);
}

Window *WorkspaceWrapper::activeClient() const
{
    return workspace()->activeWindow();
}

QString WorkspaceWrapper::currentActivity() const
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return QString();
    }
    return Workspace::self()->activities()->current();
#else
    return QString();
#endif
}

void WorkspaceWrapper::setCurrentActivity(QString activity)
{
#if KWIN_BUILD_ACTIVITIES
    if (Workspace::self()->activities()) {
        Workspace::self()->activities()->setCurrent(activity);
    }
#endif
}

QStringList WorkspaceWrapper::activityList() const
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return QStringList();
    }
    return Workspace::self()->activities()->all();
#else
    return QStringList();
#endif
}

QPoint WorkspaceWrapper::cursorPos() const
{
    return Cursors::self()->mouse()->pos();
}

#define SLOTWRAPPER(name)          \
    void WorkspaceWrapper::name()  \
    {                              \
        Workspace::self()->name(); \
    }

SLOTWRAPPER(slotToggleShowDesktop)

SLOTWRAPPER(slotWindowMaximize)
SLOTWRAPPER(slotWindowMaximizeVertical)
SLOTWRAPPER(slotWindowMaximizeHorizontal)
SLOTWRAPPER(slotWindowMinimize)
SLOTWRAPPER(slotWindowShade)
SLOTWRAPPER(slotWindowRaise)
SLOTWRAPPER(slotWindowLower)
SLOTWRAPPER(slotWindowRaiseOrLower)
SLOTWRAPPER(slotActivateAttentionWindow)
SLOTWRAPPER(slotWindowMoveLeft)
SLOTWRAPPER(slotWindowMoveRight)
SLOTWRAPPER(slotWindowMoveUp)
SLOTWRAPPER(slotWindowMoveDown)
SLOTWRAPPER(slotWindowExpandHorizontal)
SLOTWRAPPER(slotWindowExpandVertical)
SLOTWRAPPER(slotWindowShrinkHorizontal)
SLOTWRAPPER(slotWindowShrinkVertical)

SLOTWRAPPER(slotIncreaseWindowOpacity)
SLOTWRAPPER(slotLowerWindowOpacity)

SLOTWRAPPER(slotWindowOperations)
SLOTWRAPPER(slotWindowClose)
SLOTWRAPPER(slotWindowMove)
SLOTWRAPPER(slotWindowResize)
SLOTWRAPPER(slotWindowAbove)
SLOTWRAPPER(slotWindowBelow)
SLOTWRAPPER(slotWindowOnAllDesktops)
SLOTWRAPPER(slotWindowFullScreen)
SLOTWRAPPER(slotWindowNoBorder)

SLOTWRAPPER(slotWindowToNextDesktop)
SLOTWRAPPER(slotWindowToPreviousDesktop)
SLOTWRAPPER(slotWindowToDesktopRight)
SLOTWRAPPER(slotWindowToDesktopLeft)
SLOTWRAPPER(slotWindowToDesktopUp)
SLOTWRAPPER(slotWindowToDesktopDown)

SLOTWRAPPER(slotWindowToPrevScreen)
SLOTWRAPPER(slotWindowToNextScreen)
SLOTWRAPPER(slotWindowToLeftScreen)
SLOTWRAPPER(slotWindowToRightScreen)
SLOTWRAPPER(slotWindowToAboveScreen)
SLOTWRAPPER(slotWindowToBelowScreen)

SLOTWRAPPER(slotSwitchToPrevScreen)
SLOTWRAPPER(slotSwitchToNextScreen)
SLOTWRAPPER(slotSwitchToLeftScreen)
SLOTWRAPPER(slotSwitchToRightScreen)
SLOTWRAPPER(slotSwitchToAboveScreen)
SLOTWRAPPER(slotSwitchToBelowScreen)

#undef SLOTWRAPPER

#define SLOTWRAPPER(name, modes)                   \
    void WorkspaceWrapper::name()                  \
    {                                              \
        Workspace::self()->quickTileWindow(modes); \
    }

SLOTWRAPPER(slotWindowQuickTileLeft, QuickTileFlag::Left)
SLOTWRAPPER(slotWindowQuickTileRight, QuickTileFlag::Right)
SLOTWRAPPER(slotWindowQuickTileTop, QuickTileFlag::Top)
SLOTWRAPPER(slotWindowQuickTileBottom, QuickTileFlag::Bottom)
SLOTWRAPPER(slotWindowQuickTileTopLeft, QuickTileFlag::Top | QuickTileFlag::Left)
SLOTWRAPPER(slotWindowQuickTileTopRight, QuickTileFlag::Top | QuickTileFlag::Right)
SLOTWRAPPER(slotWindowQuickTileBottomLeft, QuickTileFlag::Bottom | QuickTileFlag::Left)
SLOTWRAPPER(slotWindowQuickTileBottomRight, QuickTileFlag::Bottom | QuickTileFlag::Right)

#undef SLOTWRAPPER

#define SLOTWRAPPER(name, direction)                           \
    void WorkspaceWrapper::name()                              \
    {                                                          \
        Workspace::self()->switchWindow(Workspace::direction); \
    }

SLOTWRAPPER(slotSwitchWindowUp, DirectionNorth)
SLOTWRAPPER(slotSwitchWindowDown, DirectionSouth)
SLOTWRAPPER(slotSwitchWindowRight, DirectionEast)
SLOTWRAPPER(slotSwitchWindowLeft, DirectionWest)

#undef SLOTWRAPPER

#define SLOTWRAPPER(name, direction)                                                                                       \
    void WorkspaceWrapper::name()                                                                                          \
    {                                                                                                                      \
        VirtualDesktopManager::self()->moveTo(VirtualDesktopManager::Direction::direction, options->isRollOverDesktops()); \
    }

SLOTWRAPPER(slotSwitchDesktopNext, Next)
SLOTWRAPPER(slotSwitchDesktopPrevious, Previous)
SLOTWRAPPER(slotSwitchDesktopRight, Right)
SLOTWRAPPER(slotSwitchDesktopLeft, Left)
SLOTWRAPPER(slotSwitchDesktopUp, Up)
SLOTWRAPPER(slotSwitchDesktopDown, Down)

#undef SLOTWRAPPER

void WorkspaceWrapper::setActiveClient(KWin::Window *client)
{
    KWin::Workspace::self()->activateWindow(client);
}

QSize WorkspaceWrapper::workspaceSize() const
{
    return QSize(workspaceWidth(), workspaceHeight());
}

QSize WorkspaceWrapper::displaySize() const
{
    return workspace()->geometry().size();
}

int WorkspaceWrapper::displayWidth() const
{
    return displaySize().width();
}

int WorkspaceWrapper::displayHeight() const
{
    return displaySize().height();
}

static VirtualDesktop *resolveVirtualDesktop(int desktopId)
{
    if (desktopId == 0 || desktopId == -1) {
        return VirtualDesktopManager::self()->currentDesktop();
    } else {
        return VirtualDesktopManager::self()->desktopForX11Id(desktopId);
    }
}

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, const QPoint &p, int desktop) const
{
    const Output *output = Workspace::self()->outputAt(p);
    const VirtualDesktop *virtualDesktop = resolveVirtualDesktop(desktop);
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), output, virtualDesktop);
}

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, const QPoint &p, VirtualDesktop *desktop) const
{
    return workspace()->clientArea(static_cast<clientAreaOption>(option), workspace()->outputAt(p), desktop);
}

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, const KWin::Window *c) const
{
    if (!c) {
        return QRectF();
    }
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), c);
}

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, KWin::Window *c) const
{
    if (!c) {
        return QRectF();
    }
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), c);
}

static VirtualDesktop *resolveDesktop(int desktopId)
{
    auto vdm = VirtualDesktopManager::self();
    if (desktopId == NETWinInfo::OnAllDesktops || desktopId == 0) {
        return vdm->currentDesktop();
    }
    return vdm->desktopForX11Id(desktopId);
}

static Output *resolveOutput(int outputId)
{
    if (outputId == -1) {
        return workspace()->activeOutput();
    }
    return workspace()->outputs().value(outputId);
}

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, int outputId, int desktopId) const
{
    VirtualDesktop *desktop = resolveDesktop(desktopId);
    if (Q_UNLIKELY(!desktop)) {
        return QRect();
    }

    Output *output = resolveOutput(outputId);
    if (Q_UNLIKELY(!output)) {
        return QRect();
    }

    return workspace()->clientArea(static_cast<clientAreaOption>(option), output, desktop);
}

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, Output *output, VirtualDesktop *desktop) const
{
    return workspace()->clientArea(static_cast<clientAreaOption>(option), output, desktop);
}

QString WorkspaceWrapper::desktopName(int desktop) const
{
    const VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForX11Id(desktop);
    return vd ? vd->name() : QString();
}

void WorkspaceWrapper::createDesktop(int position, const QString &name) const
{
    VirtualDesktopManager::self()->createVirtualDesktop(position, name);
}

void WorkspaceWrapper::removeDesktop(int position) const
{
    VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForX11Id(position + 1);
    if (!vd) {
        return;
    }

    VirtualDesktopManager::self()->removeVirtualDesktop(vd->id());
}

QString WorkspaceWrapper::supportInformation() const
{
    return Workspace::self()->supportInformation();
}

void WorkspaceWrapper::setupClientConnections(Window *client)
{
    connect(client, &Window::clientMinimized, this, &WorkspaceWrapper::clientMinimized);
    connect(client, &Window::clientUnminimized, this, &WorkspaceWrapper::clientUnminimized);
    connect(client, qOverload<Window *, bool, bool>(&Window::clientMaximizedStateChanged),
            this, &WorkspaceWrapper::clientMaximizeSet);

    X11Window *x11Client = qobject_cast<X11Window *>(client); // TODO: Drop X11-specific signals.
    if (!x11Client) {
        return;
    }

    connect(x11Client, &X11Window::clientManaging, this, &WorkspaceWrapper::clientManaging);
    connect(x11Client, &X11Window::clientFullScreenSet, this, &WorkspaceWrapper::clientFullScreenSet);
}

void WorkspaceWrapper::showOutline(const QRect &geometry)
{
    workspace()->outline()->show(geometry);
}

void WorkspaceWrapper::showOutline(int x, int y, int width, int height)
{
    workspace()->outline()->show(QRect(x, y, width, height));
}

void WorkspaceWrapper::hideOutline()
{
    workspace()->outline()->hide();
}

X11Window *WorkspaceWrapper::getClient(qulonglong windowId)
{
    return Workspace::self()->findClient(Predicate::WindowMatch, windowId);
}

QSize WorkspaceWrapper::desktopGridSize() const
{
    return VirtualDesktopManager::self()->grid().size();
}

int WorkspaceWrapper::desktopGridWidth() const
{
    return desktopGridSize().width();
}

int WorkspaceWrapper::desktopGridHeight() const
{
    return desktopGridSize().height();
}

int WorkspaceWrapper::workspaceHeight() const
{
    return desktopGridHeight() * displayHeight();
}

int WorkspaceWrapper::workspaceWidth() const
{
    return desktopGridWidth() * displayWidth();
}

int WorkspaceWrapper::numScreens() const
{
    return workspace()->outputs().count();
}

int WorkspaceWrapper::screenAt(const QPointF &pos) const
{
    return workspace()->outputs().indexOf(workspace()->outputAt(pos));
}

int WorkspaceWrapper::activeScreen() const
{
    return workspace()->outputs().indexOf(workspace()->activeOutput());
}

QRect WorkspaceWrapper::virtualScreenGeometry() const
{
    return workspace()->geometry();
}

QSize WorkspaceWrapper::virtualScreenSize() const
{
    return workspace()->geometry().size();
}

void WorkspaceWrapper::sendClientToScreen(Window *client, int screen)
{
    Output *output = workspace()->outputs().value(screen);
    if (output) {
        workspace()->sendWindowToOutput(client, output);
    }
}

KWin::TileManager *WorkspaceWrapper::tilingForScreen(const QString &screenName) const
{
    Output *output = kwinApp()->outputBackend()->findOutput(screenName);
    if (output) {
        return workspace()->tileManager(output);
    }
    return nullptr;
}

KWin::TileManager *WorkspaceWrapper::tilingForScreen(int screen) const
{
    Output *output = workspace()->outputs().value(screen);
    if (output) {
        return workspace()->tileManager(output);
    }
    return nullptr;
}

QtScriptWorkspaceWrapper::QtScriptWorkspaceWrapper(QObject *parent)
    : WorkspaceWrapper(parent)
{
}

QList<KWin::Window *> QtScriptWorkspaceWrapper::clientList() const
{
    return workspace()->allClientList();
}

QQmlListProperty<KWin::Window> DeclarativeScriptWorkspaceWrapper::clients()
{
    return QQmlListProperty<KWin::Window>(this, nullptr, &DeclarativeScriptWorkspaceWrapper::countClientList, &DeclarativeScriptWorkspaceWrapper::atClientList);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
int DeclarativeScriptWorkspaceWrapper::countClientList(QQmlListProperty<KWin::Window> *clients)
#else
qsizetype DeclarativeScriptWorkspaceWrapper::countClientList(QQmlListProperty<KWin::Window> *clients)
#endif
{
    return workspace()->allClientList().size();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
KWin::Window *DeclarativeScriptWorkspaceWrapper::atClientList(QQmlListProperty<KWin::Window> *clients, int index)
#else
KWin::Window *DeclarativeScriptWorkspaceWrapper::atClientList(QQmlListProperty<KWin::Window> *clients, qsizetype index)
#endif
{
    return workspace()->allClientList().at(index);
}

DeclarativeScriptWorkspaceWrapper::DeclarativeScriptWorkspaceWrapper(QObject *parent)
    : WorkspaceWrapper(parent)
{
}

} // KWin

#include "moc_workspace_wrapper.cpp"
