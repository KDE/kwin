/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "workspace_wrapper.h"
#include "outline.h"
#include "platform.h"
#include "screens.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "x11window.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

#include <QApplication>
#include <QDesktopWidget>

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
    if (KWin::Activities *activities = KWin::Activities::self()) {
        connect(activities, &Activities::currentChanged, this, &WorkspaceWrapper::currentActivityChanged);
        connect(activities, &Activities::added, this, &WorkspaceWrapper::activitiesChanged);
        connect(activities, &Activities::added, this, &WorkspaceWrapper::activityAdded);
        connect(activities, &Activities::removed, this, &WorkspaceWrapper::activitiesChanged);
        connect(activities, &Activities::removed, this, &WorkspaceWrapper::activityRemoved);
    }
#endif
    connect(screens(), &Screens::sizeChanged, this, &WorkspaceWrapper::virtualScreenSizeChanged);
    connect(screens(), &Screens::geometryChanged, this, &WorkspaceWrapper::virtualScreenGeometryChanged);
    connect(screens(), &Screens::countChanged, this, [this](int previousCount, int currentCount) {
        Q_UNUSED(previousCount)
        Q_EMIT numberScreensChanged(currentCount);
    });
    // TODO Plasma 6: Remove it.
    connect(QApplication::desktop(), &QDesktopWidget::resized, this, &WorkspaceWrapper::screenResized);

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
#if KWIN_BUILD_ACTIVITIES
    if (Activities::self()) {
        Activities::self()->setCurrent(activity);
    }
#else
    Q_UNUSED(activity)
#endif
}

QStringList WorkspaceWrapper::activityList() const
{
#if KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return QStringList();
    }
    return Activities::self()->all();
#else
    return QStringList();
#endif
}

#define SLOTWRAPPER(name)          \
    void WorkspaceWrapper::name()  \
    {                              \
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

#define SLOTWRAPPER(name, direction)                                                     \
    void WorkspaceWrapper::name()                                                        \
    {                                                                                    \
        VirtualDesktopManager::self()->moveTo<direction>(options->isRollOverDesktops()); \
    }

SLOTWRAPPER(slotSwitchDesktopNext, DesktopNext)
SLOTWRAPPER(slotSwitchDesktopPrevious, DesktopPrevious)
SLOTWRAPPER(slotSwitchDesktopRight, DesktopRight)
SLOTWRAPPER(slotSwitchDesktopLeft, DesktopLeft)
SLOTWRAPPER(slotSwitchDesktopUp, DesktopAbove)
SLOTWRAPPER(slotSwitchDesktopDown, DesktopBelow)

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

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, const QPoint &p, int desktop) const
{
    const Output *output = kwinApp()->platform()->outputAt(p);
    const VirtualDesktop *virtualDesktop = resolveVirtualDesktop(desktop);
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), output, virtualDesktop);
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, const QPoint &p, VirtualDesktop *desktop) const
{
    return workspace()->clientArea(static_cast<clientAreaOption>(option), kwinApp()->platform()->outputAt(p), desktop);
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, const KWin::Window *c) const
{
    return Workspace::self()->clientArea(static_cast<clientAreaOption>(option), c);
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, KWin::Window *c) const
{
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
    return kwinApp()->platform()->findOutput(outputId);
}

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, int outputId, int desktopId) const
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

QRect WorkspaceWrapper::clientArea(ClientAreaOption option, Output *output, VirtualDesktop *desktop) const
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
    return screens()->count();
}

int WorkspaceWrapper::screenAt(const QPointF &pos) const
{
    return kwinApp()->platform()->enabledOutputs().indexOf(kwinApp()->platform()->outputAt(pos.toPoint()));
}

int WorkspaceWrapper::activeScreen() const
{
    return kwinApp()->platform()->enabledOutputs().indexOf(workspace()->activeOutput());
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
    Output *output = kwinApp()->platform()->findOutput(screen);
    if (output) {
        workspace()->sendWindowToOutput(client, output);
    }
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
    Q_UNUSED(clients)
    return workspace()->allClientList().size();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
KWin::Window *DeclarativeScriptWorkspaceWrapper::atClientList(QQmlListProperty<KWin::Window> *clients, int index)
#else
KWin::Window *DeclarativeScriptWorkspaceWrapper::atClientList(QQmlListProperty<KWin::Window> *clients, qsizetype index)
#endif
{
    Q_UNUSED(clients)
    return workspace()->allClientList().at(index);
}

DeclarativeScriptWorkspaceWrapper::DeclarativeScriptWorkspaceWrapper(QObject *parent)
    : WorkspaceWrapper(parent)
{
}

} // KWin

#include "moc_workspace_wrapper.cpp"
