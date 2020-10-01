/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "workspace_wrapper.h"
#include "../x11client.h"
#include "../outline.h"
#include "../screens.h"
#include "../virtualdesktops.h"
#include "../workspace.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "../activities.h"
#endif

#include <QDesktopWidget>
#include <QApplication>

namespace KWin {

WorkspaceWrapper::WorkspaceWrapper(QObject* parent) : QObject(parent)
{
    KWin::Workspace *ws = KWin::Workspace::self();
    KWin::VirtualDesktopManager *vds = KWin::VirtualDesktopManager::self();
    connect(ws, &Workspace::desktopPresenceChanged, this, &WorkspaceWrapper::desktopPresenceChanged);
    connect(ws, &Workspace::currentDesktopChanged, this, &WorkspaceWrapper::currentDesktopChanged);
    connect(ws, &Workspace::clientAdded, this, &WorkspaceWrapper::clientAdded);
    connect(ws, &Workspace::clientAdded, this, &WorkspaceWrapper::setupClientConnections);
    connect(ws, &Workspace::clientRemoved, this, &WorkspaceWrapper::clientRemoved);
    connect(ws, &Workspace::clientActivated, this, &WorkspaceWrapper::clientActivated);
    connect(vds, SIGNAL(countChanged(uint,uint)), SIGNAL(numberDesktopsChanged(uint)));
    connect(vds, SIGNAL(layoutChanged(int,int)), SIGNAL(desktopLayoutChanged()));
    connect(ws, &Workspace::clientDemandsAttentionChanged, this, &WorkspaceWrapper::clientDemandsAttentionChanged);
#ifdef KWIN_BUILD_ACTIVITIES
    if (KWin::Activities *activities = KWin::Activities::self()) {
        connect(activities, SIGNAL(currentChanged(QString)), SIGNAL(currentActivityChanged(QString)));
        connect(activities, SIGNAL(added(QString)), SIGNAL(activitiesChanged(QString)));
        connect(activities, SIGNAL(added(QString)), SIGNAL(activityAdded(QString)));
        connect(activities, SIGNAL(removed(QString)), SIGNAL(activitiesChanged(QString)));
        connect(activities, SIGNAL(removed(QString)), SIGNAL(activityRemoved(QString)));
    }
#endif
    connect(screens(), &Screens::sizeChanged, this, &WorkspaceWrapper::virtualScreenSizeChanged);
    connect(screens(), &Screens::geometryChanged, this, &WorkspaceWrapper::virtualScreenGeometryChanged);
    connect(screens(), &Screens::countChanged, this,
        [this] (int previousCount, int currentCount) {
            Q_UNUSED(previousCount)
            emit numberScreensChanged(currentCount);
        }
    );
    connect(QApplication::desktop(), SIGNAL(resized(int)), SIGNAL(screenResized(int)));

    const QList<AbstractClient *> clients = ws->allClientList();
    for (AbstractClient *client : clients) {
        setupClientConnections(client);
    }
}

int WorkspaceWrapper::currentDesktop() const
{
    return VirtualDesktopManager::self()->current();
}

int WorkspaceWrapper::numberOfDesktops() const
{
    return VirtualDesktopManager::self()->count();
}

void WorkspaceWrapper::setCurrentDesktop(int desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

void WorkspaceWrapper::setNumberOfDesktops(int count)
{
    VirtualDesktopManager::self()->setCount(count);
}

AbstractClient *WorkspaceWrapper::activeClient() const
{
    return workspace()->activeClient();
}

QString WorkspaceWrapper::currentActivity() const
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return QString();
    }
    return Activities::self()->current();
#else
    return QString();
#endif
}

void WorkspaceWrapper::setCurrentActivity(QString activity)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (Activities::self()) {
        Activities::self()->setCurrent(activity);
    }
#else
    Q_UNUSED(activity)
#endif
}

QStringList WorkspaceWrapper::activityList() const
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return QStringList();
    }
    return Activities::self()->all();
#else
    return QStringList();
#endif
}

#define SLOTWRAPPER(name) \
void WorkspaceWrapper::name( ) { \
    Workspace::self()->name(); \
}

SLOTWRAPPER(slotSwitchToNextScreen)
SLOTWRAPPER(slotWindowToNextScreen)
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
SLOTWRAPPER(slotWindowPackLeft)
SLOTWRAPPER(slotWindowPackRight)
SLOTWRAPPER(slotWindowPackUp)
SLOTWRAPPER(slotWindowPackDown)
SLOTWRAPPER(slotWindowGrowHorizontal)
SLOTWRAPPER(slotWindowGrowVertical)
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

#undef SLOTWRAPPER

#define SLOTWRAPPER(name,modes) \
void WorkspaceWrapper::name() { \
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

#define SLOTWRAPPER(name,direction) \
void WorkspaceWrapper::name() { \
    Workspace::self()->switchWindow(Workspace::direction); \
}

SLOTWRAPPER(slotSwitchWindowUp, DirectionNorth)
SLOTWRAPPER(slotSwitchWindowDown, DirectionSouth)
SLOTWRAPPER(slotSwitchWindowRight, DirectionEast)
SLOTWRAPPER(slotSwitchWindowLeft, DirectionWest)

#undef SLOTWRAPPER

#define SLOTWRAPPER(name,direction) \
void WorkspaceWrapper::name( ) { \
    VirtualDesktopManager::self()->moveTo<direction>(options->isRollOverDesktops()); \
}

SLOTWRAPPER(slotSwitchDesktopNext,DesktopNext)
SLOTWRAPPER(slotSwitchDesktopPrevious,DesktopPrevious)
SLOTWRAPPER(slotSwitchDesktopRight,DesktopRight)
SLOTWRAPPER(slotSwitchDesktopLeft,DesktopLeft)
SLOTWRAPPER(slotSwitchDesktopUp,DesktopAbove)
SLOTWRAPPER(slotSwitchDesktopDown,DesktopBelow)

#undef SLOTWRAPPER

void WorkspaceWrapper::setActiveClient(KWin::AbstractClient* client)
{
    KWin::Workspace::self()->activateClient(client);
}

QSize WorkspaceWrapper::workspaceSize() const
{
    return QSize(workspaceWidth(), workspaceHeight());
}

QSize WorkspaceWrapper::displaySize() const
{
    return screens()->displaySize();
}

int WorkspaceWrapper::displayWidth() const
{
    return displaySize().width();
}

int WorkspaceWrapper::displayHeight() const
{
    return displaySize().height();
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, const QPoint &p, int desktop) const
{
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), p, desktop);
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, const KWin::AbstractClient *c) const
{
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), c);
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, int screen, int desktop) const
{
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), screen, desktop);
}

QString WorkspaceWrapper::desktopName(int desktop) const
{
    return VirtualDesktopManager::self()->name(desktop);
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

void WorkspaceWrapper::setupClientConnections(AbstractClient *client)
{
    connect(client, &AbstractClient::clientMinimized, this, &WorkspaceWrapper::clientMinimized);
    connect(client, &AbstractClient::clientUnminimized, this, &WorkspaceWrapper::clientUnminimized);
    connect(client, qOverload<AbstractClient *, bool, bool>(&AbstractClient::clientMaximizedStateChanged),
            this, &WorkspaceWrapper::clientMaximizeSet);

    X11Client *x11Client = qobject_cast<X11Client *>(client); // TODO: Drop X11-specific signals.
    if (!x11Client)
        return;

    connect(x11Client, &X11Client::clientManaging, this, &WorkspaceWrapper::clientManaging);
    connect(x11Client, &X11Client::clientFullScreenSet, this, &WorkspaceWrapper::clientFullScreenSet);
}

void WorkspaceWrapper::showOutline(const QRect &geometry)
{
    outline()->show(geometry);
}

void WorkspaceWrapper::showOutline(int x, int y, int width, int height)
{
    outline()->show(QRect(x, y, width, height));
}

void WorkspaceWrapper::hideOutline()
{
    outline()->hide();
}

X11Client *WorkspaceWrapper::getClient(qulonglong windowId)
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
    return screens()->count();
}

int WorkspaceWrapper::activeScreen() const
{
    return screens()->current();
}

QRect WorkspaceWrapper::virtualScreenGeometry() const
{
    return screens()->geometry();
}

QSize WorkspaceWrapper::virtualScreenSize() const
{
    return screens()->size();
}

void WorkspaceWrapper::sendClientToScreen(AbstractClient *client, int screen)
{
    if (screen < 0 || screen >= screens()->count()) {
        return;
    }
    workspace()->sendClientToScreen(client, screen);
}

QtScriptWorkspaceWrapper::QtScriptWorkspaceWrapper(QObject* parent)
    : WorkspaceWrapper(parent) {}

QList<KWin::AbstractClient *> QtScriptWorkspaceWrapper::clientList() const
{
    return workspace()->allClientList();
}

QQmlListProperty<KWin::AbstractClient> DeclarativeScriptWorkspaceWrapper::clients()
{
    return QQmlListProperty<KWin::AbstractClient>(this, nullptr, &DeclarativeScriptWorkspaceWrapper::countClientList, &DeclarativeScriptWorkspaceWrapper::atClientList);
}

int DeclarativeScriptWorkspaceWrapper::countClientList(QQmlListProperty<KWin::AbstractClient> *clients)
{
    Q_UNUSED(clients)
    return workspace()->allClientList().size();
}

KWin::AbstractClient *DeclarativeScriptWorkspaceWrapper::atClientList(QQmlListProperty<KWin::AbstractClient> *clients, int index)
{
    Q_UNUSED(clients)
    return workspace()->allClientList().at(index);
}

DeclarativeScriptWorkspaceWrapper::DeclarativeScriptWorkspaceWrapper(QObject* parent)
    : WorkspaceWrapper(parent) {}

} // KWin
