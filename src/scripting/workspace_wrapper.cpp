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

namespace KWin
{

WorkspaceWrapper::WorkspaceWrapper(QObject *parent)
    : QObject(parent)
{
    KWin::Workspace *ws = KWin::Workspace::self();
    KWin::VirtualDesktopManager *vds = KWin::VirtualDesktopManager::self();
    connect(ws, &Workspace::desktopPresenceChanged, this, &WorkspaceWrapper::desktopPresenceChanged);
    connect(ws, &Workspace::windowAdded, this, &WorkspaceWrapper::clientAdded);
    connect(ws, &Workspace::windowRemoved, this, &WorkspaceWrapper::clientRemoved);
    connect(ws, &Workspace::windowActivated, this, &WorkspaceWrapper::clientActivated);
    connect(vds, &VirtualDesktopManager::desktopCreated, this, &WorkspaceWrapper::desktopsChanged);
    connect(vds, &VirtualDesktopManager::desktopRemoved, this, &WorkspaceWrapper::desktopsChanged);
    connect(vds, &VirtualDesktopManager::layoutChanged, this, &WorkspaceWrapper::desktopLayoutChanged);
    connect(vds, &VirtualDesktopManager::currentChanged, this, &WorkspaceWrapper::currentDesktopChanged);
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
    connect(ws, &Workspace::outputsChanged, this, &WorkspaceWrapper::screensChanged);
    connect(Cursors::self()->mouse(), &Cursor::posChanged, this, &WorkspaceWrapper::cursorPosChanged);
}

VirtualDesktop *WorkspaceWrapper::currentDesktop() const
{
    return VirtualDesktopManager::self()->currentDesktop();
}

QVector<VirtualDesktop *> WorkspaceWrapper::desktops() const
{
    return VirtualDesktopManager::self()->desktops();
}

void WorkspaceWrapper::setCurrentDesktop(VirtualDesktop *desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
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
    return Cursors::self()->mouse()->pos().toPoint();
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

QRectF WorkspaceWrapper::clientArea(ClientAreaOption option, Output *output, VirtualDesktop *desktop) const
{
    return workspace()->clientArea(static_cast<clientAreaOption>(option), output, desktop);
}

void WorkspaceWrapper::createDesktop(int position, const QString &name) const
{
    VirtualDesktopManager::self()->createVirtualDesktop(position, name);
}

void WorkspaceWrapper::removeDesktop(VirtualDesktop *desktop) const
{
    VirtualDesktopManager::self()->removeVirtualDesktop(desktop->id());
}

QString WorkspaceWrapper::supportInformation() const
{
    return Workspace::self()->supportInformation();
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

Window *WorkspaceWrapper::getClient(qulonglong windowId)
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
    return desktopGridHeight() * workspace()->geometry().height();
}

int WorkspaceWrapper::workspaceWidth() const
{
    return desktopGridWidth() * workspace()->geometry().width();
}

Output *WorkspaceWrapper::activeScreen() const
{
    return workspace()->activeOutput();
}

QList<Output *> WorkspaceWrapper::screens() const
{
    return workspace()->outputs();
}

Output *WorkspaceWrapper::screenAt(const QPointF &pos) const
{
    return workspace()->outputAt(pos);
}

QRect WorkspaceWrapper::virtualScreenGeometry() const
{
    return workspace()->geometry();
}

QSize WorkspaceWrapper::virtualScreenSize() const
{
    return workspace()->geometry().size();
}

void WorkspaceWrapper::sendClientToScreen(Window *client, Output *output)
{
    workspace()->sendWindowToOutput(client, output);
}

KWin::TileManager *WorkspaceWrapper::tilingForScreen(const QString &screenName) const
{
    Output *output = kwinApp()->outputBackend()->findOutput(screenName);
    if (output) {
        return workspace()->tileManager(output);
    }
    return nullptr;
}

KWin::TileManager *WorkspaceWrapper::tilingForScreen(Output *output) const
{
    return workspace()->tileManager(output);
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
