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
#include "effect/effecthandler.h"
#include "outline.h"
#include "scripting_logging.h"
#include "tiles/tilemanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#if KWIN_BUILD_X11
#include "x11window.h"
#endif

#include <QJSEngine>

namespace KWin
{

WorkspaceWrapper::WorkspaceWrapper(QObject *parent)
    : QObject(parent)
{
    KWin::Workspace *ws = KWin::Workspace::self();
    KWin::VirtualDesktopManager *vds = KWin::VirtualDesktopManager::self();
    connect(ws, &Workspace::windowAdded, this, &WorkspaceWrapper::windowAdded);
    connect(ws, &Workspace::windowRemoved, this, &WorkspaceWrapper::windowRemoved);
    connect(ws, &Workspace::windowActivated, this, &WorkspaceWrapper::windowActivated);
    connect(vds, &VirtualDesktopManager::desktopAdded, this, &WorkspaceWrapper::desktopsChanged);
    connect(vds, &VirtualDesktopManager::desktopRemoved, this, &WorkspaceWrapper::desktopsChanged);
    connect(vds, &VirtualDesktopManager::layoutChanged, this, &WorkspaceWrapper::desktopLayoutChanged);
    connect(vds, &VirtualDesktopManager::currentChanged, this, &WorkspaceWrapper::currentDesktopChanged);
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
    connect(ws, &Workspace::outputOrderChanged, this, &WorkspaceWrapper::screenOrderChanged);
    connect(Cursors::self()->mouse(), &Cursor::posChanged, this, &WorkspaceWrapper::cursorPosChanged);
}

VirtualDesktop *WorkspaceWrapper::currentDesktop() const
{
    return VirtualDesktopManager::self()->currentDesktop();
}

QList<VirtualDesktop *> WorkspaceWrapper::desktops() const
{
    return VirtualDesktopManager::self()->desktops();
}

void WorkspaceWrapper::setCurrentDesktop(VirtualDesktop *desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

Window *WorkspaceWrapper::activeWindow() const
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

void WorkspaceWrapper::setCurrentActivity(const QString &activity)
{
#if KWIN_BUILD_ACTIVITIES
    if (Workspace::self()->activities()) {
        Workspace::self()->activities()->setCurrent(activity, nullptr);
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

void WorkspaceWrapper::setActiveWindow(KWin::Window *window)
{
    KWin::Workspace::self()->activateWindow(window);
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
    if (!output || !desktop) {
        qCWarning(KWIN_SCRIPTING) << "clientArea needs valid output:" << output << "and desktop:" << desktop << "arguments";
        return QRect();
    }
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

QList<KWin::Window *> WorkspaceWrapper::stackingOrder() const
{
    return workspace()->stackingOrder();
}

void WorkspaceWrapper::raiseWindow(KWin::Window *window)
{
    if (window) {
        KWin::Workspace::self()->raiseWindow(window);
    }
}

#if KWIN_BUILD_X11
Window *WorkspaceWrapper::getClient(qulonglong windowId)
{
    auto window = Workspace::self()->findClient(Predicate::WindowMatch, windowId);
    QJSEngine::setObjectOwnership(window, QJSEngine::CppOwnership);
    return window;
}
#endif

QList<KWin::Window *> WorkspaceWrapper::windowAt(const QPointF &pos, int count) const
{
    QList<KWin::Window *> result;
    int found = 0;
    const QList<Window *> &stacking = workspace()->stackingOrder();
    if (stacking.isEmpty()) {
        return result;
    }
    auto it = stacking.end();
    do {
        if (found == count) {
            return result;
        }
        --it;
        Window *window = (*it);
        if (window->isDeleted()) {
            continue;
        }
        if (!window->isOnCurrentActivity() || !window->isOnCurrentDesktop() || window->isMinimized() || window->isHidden() || window->isHiddenByShowDesktop()) {
            continue;
        }
        if (window->hitTest(pos)) {
            result.append(window);
            found++;
        }
    } while (it != stacking.begin());
    return result;
}

bool WorkspaceWrapper::isEffectActive(const QString &pluginId) const
{
    if (!effects) {
        return false;
    }
    return effects->isEffectActive(pluginId);
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

QList<Output *> WorkspaceWrapper::screenOrder() const
{
    return workspace()->outputOrder();
}

Output *WorkspaceWrapper::screenAt(const QPointF &pos) const
{
    auto output = workspace()->outputAt(pos);
    QJSEngine::setObjectOwnership(output, QJSEngine::CppOwnership);
    return output;
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
    client->sendToOutput(output);
}

KWin::TileManager *WorkspaceWrapper::tilingForScreen(const QString &screenName) const
{
    Output *output = kwinApp()->outputBackend()->findOutput(screenName);
    if (output) {
        auto tileManager = workspace()->tileManager(output);
        QJSEngine::setObjectOwnership(tileManager, QJSEngine::CppOwnership);
        return tileManager;
    }
    return nullptr;
}

KWin::TileManager *WorkspaceWrapper::tilingForScreen(Output *output) const
{
    auto tileManager = workspace()->tileManager(output);
    QJSEngine::setObjectOwnership(tileManager, QJSEngine::CppOwnership);
    return tileManager;
}

QtScriptWorkspaceWrapper::QtScriptWorkspaceWrapper(QObject *parent)
    : WorkspaceWrapper(parent)
{
}

QList<KWin::Window *> QtScriptWorkspaceWrapper::windowList() const
{
    return workspace()->windows();
}

QQmlListProperty<KWin::Window> DeclarativeScriptWorkspaceWrapper::windows()
{
    return QQmlListProperty<KWin::Window>(this, nullptr, &DeclarativeScriptWorkspaceWrapper::countWindowList, &DeclarativeScriptWorkspaceWrapper::atWindowList);
}

qsizetype DeclarativeScriptWorkspaceWrapper::countWindowList(QQmlListProperty<KWin::Window> *windows)
{
    return workspace()->windows().size();
}

KWin::Window *DeclarativeScriptWorkspaceWrapper::atWindowList(QQmlListProperty<KWin::Window> *windows, qsizetype index)
{
    return workspace()->windows().at(index);
}

DeclarativeScriptWorkspaceWrapper::DeclarativeScriptWorkspaceWrapper(QObject *parent)
    : WorkspaceWrapper(parent)
{
}

} // KWin

#include "moc_workspace_wrapper.cpp"
