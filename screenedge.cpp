/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

Since the functionality provided in this class has been moved from
class Workspace, it is not clear who exactly has written the code.
The list below contains the copyright holders of the class Workspace.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "screenedge.h"

// KWin
#include "atoms.h"
#include "client.h"
#include "effects.h"
#include "utils.h"
#include "workspace.h"
#include "virtualdesktops.h"
#include "xcbutils.h"
// Qt
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCall>
#include <QDesktopWidget>

namespace KWin {

// Reset timeout
static const int TRESHOLD_RESET = 250;
// Mouse should not move more than this many pixels
static const int DISTANCE_RESET = 30;

Edge::Edge(ScreenEdges *parent)
    : QObject(parent)
    , m_edges(parent)
    , m_border(ElectricNone)
    , m_action(ElectricActionNone)
    , m_reserved(0)
{
}

Edge::~Edge()
{
}

void Edge::reserve()
{
    m_reserved++;
    if (m_reserved == 1) {
        // got activated
        activate();
    }
}

void Edge::unreserve()
{
    m_reserved--;
    if (m_reserved == 0) {
        // got deactivated
        deactivate();
    }
}

bool Edge::triggersFor(const QPoint &cursorPos) const
{
    if (!m_geometry.contains(cursorPos)) {
        return false;
    }
    if (isLeft() && cursorPos.x() != m_geometry.x()) {
        return false;
    }
    if (isRight() && cursorPos.x() != (m_geometry.x() + m_geometry.width() -1)) {
        return false;
    }
    if (isTop() && cursorPos.y() != m_geometry.y()) {
        return false;
    }
    if (isBottom() && cursorPos.y() != (m_geometry.y() + m_geometry.height() -1)) {
        return false;
    }
    return true;
}

void Edge::check(const QPoint &cursorPos, const QDateTime &triggerTime, bool forceNoPushBack)
{
    if (!triggersFor(cursorPos)) {
        return;
    }
    // no pushback so we have to activate at once
    bool directActivate = forceNoPushBack || edges()->cursorPushBackDistance().isNull();
    if (directActivate || canActivate(cursorPos, triggerTime)) {
        m_lastTrigger = triggerTime;
        m_lastReset = triggerTime;
        handle(cursorPos);
    } else {
        pushCursorBack(cursorPos);
    }
    m_triggeredPoint = cursorPos;
}

bool Edge::canActivate(const QPoint &cursorPos, const QDateTime &triggerTime)
{
    if (m_lastReset.msecsTo(triggerTime) > TRESHOLD_RESET) {
        m_lastReset = triggerTime;
        return false;
    }
    if (m_lastTrigger.msecsTo(triggerTime) < edges()->reActivationThreshold()) {
        return false;
    }
    if (m_lastReset.msecsTo(triggerTime) < edges()->timeThreshold()) {
        return false;
    }
    // does the check on position make any sense at all?
    if ((cursorPos - m_triggeredPoint).manhattanLength() > DISTANCE_RESET) {
        return false;
    }
    return true;
}

void Edge::handle(const QPoint &cursorPos)
{
    if ((edges()->isDesktopSwitchingMovingClients() && Workspace::self()->getMovingClient()) ||
        (edges()->isDesktopSwitching() && isScreenEdge())) {
        // always switch desktops in case:
        // moving a Client and option for switch on client move is enabled
        // or switch on screen edge is enabled
        switchDesktop(cursorPos);
        return;
    }
    if (handleAction() || handleByEffects()) {
        pushCursorBack(cursorPos);
        return;
    }
    if (edges()->isDesktopSwitching() && isCorner()) {
        // try again desktop switching for the corner
        switchDesktop(cursorPos);
        return;
    }
    // fallback notify world that edge got activated
    emit activated(m_border);
}

bool Edge::handleAction()
{
    switch (m_action) {
    case ElectricActionDashboard: { // Display Plasma dashboard
        QDBusInterface plasmaApp("org.kde.plasma-desktop", "/App");
        plasmaApp.asyncCall("toggleDashboard");
        return true;
    }
    case ElectricActionShowDesktop: {
        Workspace::self()->setShowingDesktop(!Workspace::self()->showingDesktop());
        return true;
    }
    case ElectricActionLockScreen: { // Lock the screen
        QDBusInterface screenSaver("org.kde.screensaver", "/ScreenSaver");
        screenSaver.asyncCall("Lock");
        return true;
    }
    default:
        return false;
    }
}

bool Edge::handleByEffects()
{
    if (!effects) {
        return false;
    }
    return static_cast<EffectsHandlerImpl*>(effects)->borderActivated(m_border);
}

void Edge::switchDesktop(const QPoint &cursorPos)
{
    QPoint pos(cursorPos);
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    uint desktop = vds->current();
    const uint oldDesktop = vds->current();
    const int OFFSET = 2;
    if (isLeft()) {
        desktop = vds->toLeft(desktop, vds->isNavigationWrappingAround());
        pos.setX(displayWidth() - 1 - OFFSET);
    }
    if (isRight()) {
        desktop = vds->toRight(desktop, vds->isNavigationWrappingAround());
        pos.setX(OFFSET);
    }
    if (isTop()) {
        desktop = vds->above(desktop, vds->isNavigationWrappingAround());
        pos.setY(displayHeight() - 1 - OFFSET);
    }
    if (isBottom()) {
        desktop = vds->below(desktop, vds->isNavigationWrappingAround());
        pos.setY(OFFSET);
    }
    if (Client *c = Workspace::self()->getMovingClient()) {
        if (c->rules()->checkDesktop(desktop) != int(desktop)) {
            // user attempts to move a client to another desktop where it is ruleforced to not be
            return;
        }
    }
    vds->setCurrent(desktop);
    if (vds->current() != oldDesktop) {
        QCursor::setPos(pos);
    }
}

void Edge::pushCursorBack(const QPoint &cursorPos)
{
    int x = cursorPos.x();
    int y = cursorPos.y();
    const QSize &distance = edges()->cursorPushBackDistance();
    if (isLeft()) {
        x += distance.width();
    }
    if (isRight()) {
        x -= distance.width();
    }
    if (isTop()) {
        y += distance.height();
    }
    if (isBottom()) {
        y -= distance.height();
    }
    QCursor::setPos(x, y);
}

void Edge::doGeometryUpdate()
{
}

void Edge::activate()
{
}

void Edge::deactivate()
{
}

/**********************************************************
 * ScreenEdges
 *********************************************************/
WindowBasedEdge::WindowBasedEdge(ScreenEdges *parent)
    : Edge(parent)
    , m_window(XCB_WINDOW_NONE)
{
}

WindowBasedEdge::~WindowBasedEdge()
{
    destroyWindow();
}

void WindowBasedEdge::activate()
{
    createWindow();
}

void WindowBasedEdge::deactivate()
{
    destroyWindow();
}

void WindowBasedEdge::createWindow()
{
    if (m_window != XCB_WINDOW_NONE) {
        return;
    }
    const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    const uint32_t values[] = {
        true,
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW
    };
    m_window = Xcb::createInputWindow(geometry(), mask, values);
    xcb_map_window(connection(), m_window);
    // Set XdndAware on the windows, so that DND enter events are received (#86998)
    xcb_atom_t version = 4; // XDND version
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, m_window,
                        atoms->xdnd_aware, XCB_ATOM_ATOM, 32, 1, (unsigned char*)(&version));
}

void WindowBasedEdge::destroyWindow()
{
    if (m_window != XCB_WINDOW_NONE) {
        xcb_destroy_window(connection(), m_window);
        m_window = XCB_WINDOW_NONE;
    }
}

void WindowBasedEdge::doGeometryUpdate()
{
    Xcb::moveResizeWindow(m_window, geometry());
}

/**********************************************************
 * ScreenEdges
 *********************************************************/
ScreenEdges::ScreenEdges(QObject *parent)
    : QObject(parent)
    , m_desktopSwitching(false)
    , m_desktopSwitchingMovingClients(false)
    , m_timeThreshold(0)
    , m_reactivateThreshold(0)
    , m_virtualDesktopLayout(0)
    , m_actionTopLeft(ElectricActionNone)
    , m_actionTop(ElectricActionNone)
    , m_actionTopRight(ElectricActionNone)
    , m_actionRight(ElectricActionNone)
    , m_actionBottomRight(ElectricActionNone)
    , m_actionBottom(ElectricActionNone)
    , m_actionBottomLeft(ElectricActionNone)
    , m_actionLeft(ElectricActionNone)
{
    m_externalReservations.insert(ElectricTopLeft, 0);
    m_externalReservations.insert(ElectricTop, 0);
    m_externalReservations.insert(ElectricTopRight, 0);
    m_externalReservations.insert(ElectricRight, 0);
    m_externalReservations.insert(ElectricBottomRight, 0);
    m_externalReservations.insert(ElectricBottom, 0);
    m_externalReservations.insert(ElectricBottomLeft, 0);
    m_externalReservations.insert(ElectricLeft, 0);
}

ScreenEdges::~ScreenEdges()
{
}

void ScreenEdges::init()
{
    reconfigure();
    updateLayout();
    recreateEdges();
}
static ElectricBorderAction electricBorderAction(const QString& name)
{
    QString lowerName = name.toLower();
    if (lowerName == "dashboard") {
        return ElectricActionDashboard;
    } else if (lowerName == "showdesktop") {
        return ElectricActionShowDesktop;
    } else if (lowerName == "lockscreen") {
        return ElectricActionLockScreen;
    } else if (lowerName == "preventscreenlocking") {
        return ElectricActionPreventScreenLocking;
    }
    return ElectricActionNone;
}

void ScreenEdges::reconfigure()
{
    if (!m_config) {
        return;
    }
    // TODO: migrate settings to a group ScreenEdges
    KConfigGroup windowsConfig = m_config->group("Windows");
    setTimeThreshold(windowsConfig.readEntry("ElectricBorderDelay", 150));
    setReActivationThreshold(windowsConfig.readEntry("ElectricBorderCooldown", 350));
    int desktopSwitching = windowsConfig.readEntry("ElectricBorders", static_cast<int>(ElectricDisabled));
    if (desktopSwitching == ElectricDisabled) {
        setDesktopSwitching(false);
        setDesktopSwitchingMovingClients(false);
    } else if (desktopSwitching == ElectricMoveOnly) {
        setDesktopSwitching(false);
        setDesktopSwitchingMovingClients(true);
    } else if (desktopSwitching == ElectricAlways) {
        setDesktopSwitching(true);
        setDesktopSwitchingMovingClients(true);
    }
    const int pushBack = windowsConfig.readEntry("ElectricBorderPushbackPixels", 1);
    m_cursorPushBackDistance = QSize(pushBack, pushBack);

    KConfigGroup borderConfig = m_config->group("ElectricBorders");
    setActionForBorder(ElectricTopLeft,     &m_actionTopLeft,
                       electricBorderAction(borderConfig.readEntry("TopLeft", "None")));
    setActionForBorder(ElectricTop,         &m_actionTop,
                       electricBorderAction(borderConfig.readEntry("Top", "None")));
    setActionForBorder(ElectricTopRight,    &m_actionTopRight,
                       electricBorderAction(borderConfig.readEntry("TopRight", "None")));
    setActionForBorder(ElectricRight,       &m_actionRight,
                       electricBorderAction(borderConfig.readEntry("Right", "None")));
    setActionForBorder(ElectricBottomRight, &m_actionBottomRight,
                       electricBorderAction(borderConfig.readEntry("BottomRight", "None")));
    setActionForBorder(ElectricBottom,      &m_actionBottom,
                       electricBorderAction(borderConfig.readEntry("Bottom", "None")));
    setActionForBorder(ElectricBottomLeft,  &m_actionBottomLeft,
                       electricBorderAction(borderConfig.readEntry("BottomLeft", "None")));
    setActionForBorder(ElectricLeft,        &m_actionLeft,
                       electricBorderAction(borderConfig.readEntry("Left", "None")));
}

void ScreenEdges::setActionForBorder(ElectricBorder border, ElectricBorderAction *oldValue, ElectricBorderAction newValue)
{
    if (*oldValue == newValue) {
        return;
    }
    if (*oldValue == ElectricActionNone) {
        // have to reserve
        for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
            if ((*it)->border() == border) {
                (*it)->reserve();
            }
        }
    }
    if (newValue == ElectricActionNone) {
        // have to unreserve
        for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
            if ((*it)->border() == border) {
                (*it)->unreserve();
            }
        }
    }
    *oldValue = newValue;
    // update action on all Edges for given border
    for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->setAction(newValue);
        }
    }
}

void ScreenEdges::updateLayout()
{
    const QSize desktopMatrix = VirtualDesktopManager::self()->grid().size();
    Qt::Orientations newLayout = 0;
    if (desktopMatrix.width() > 1) {
        newLayout |= Qt::Horizontal;
    }
    if (desktopMatrix.height() > 1) {
        newLayout |= Qt::Vertical;
    }
    if (newLayout == m_virtualDesktopLayout) {
        return;
    }
    if (isDesktopSwitching()) {
        reserveDesktopSwitching(false, m_virtualDesktopLayout);
    }
    m_virtualDesktopLayout = newLayout;
    if (isDesktopSwitching()) {
        reserveDesktopSwitching(true, m_virtualDesktopLayout);
    }
}

static bool isLeftScreen(const QRect &screen, const QRect &fullArea)
{
    const QDesktopWidget *desktop = QApplication::desktop();
    if (desktop->screenCount() == 1) {
        return true;
    }
    if (screen.x() == fullArea.x()) {
        return true;
    }
    // the screen is also on the left in case of a vertical layout with a second screen
    // more to the left. In that case no screen ends left of screen's x coord
    for (int i=0; i<desktop->screenCount(); ++i) {
        const QRect otherGeo = desktop->screenGeometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.x() + otherGeo.width() <= screen.x()) {
            // other screen is completely in the left
            return false;
        }
    }
    // did not find a screen left of our current screen, so it is the left most
    return true;
}

static bool isRightScreen(const QRect &screen, const QRect &fullArea)
{
    const QDesktopWidget *desktop = QApplication::desktop();
    if (desktop->screenCount() == 1) {
        return true;
    }
    if (screen.x() + screen.width() == fullArea.x() + fullArea.width()) {
        return true;
    }
    // the screen is also on the right in case of a vertical layout with a second screen
    // more to the right. In that case no screen starts right of this screen
    for (int i=0; i<desktop->screenCount(); ++i) {
        const QRect otherGeo = desktop->screenGeometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.x() >= screen.x() + screen.width()) {
            // other screen is completely in the right
            return false;
        }
    }
    // did not find a screen right of our current screen, so it is the right most
    return true;
}

static bool isTopScreen(const QRect &screen, const QRect &fullArea)
{
    const QDesktopWidget *desktop = QApplication::desktop();
    if (desktop->screenCount() == 1) {
        return true;
    }
    if (screen.y() == fullArea.y()) {
        return true;
    }
    // the screen is also top most in case of a horizontal layout with a second screen
    // more to the top. In that case no screen ends above screen's y coord
    for (int i=0; i<desktop->screenCount(); ++i) {
        const QRect otherGeo = desktop->screenGeometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.y() + otherGeo.height() <= screen.y()) {
            // other screen is completely above
            return false;
        }
    }
    // did not find a screen above our current screen, so it is the top most
    return true;
}

static bool isBottomScreen(const QRect &screen, const QRect &fullArea)
{
    const QDesktopWidget *desktop = QApplication::desktop();
    if (desktop->screenCount() == 1) {
        return true;
    }
    if (screen.y() + screen.height() == fullArea.y() + fullArea.height()) {
        return true;
    }
    // the screen is also bottom most in case of a horizontal layout with a second screen
    // more below. In that case no screen starts below screen's y coord + height
    for (int i=0; i<desktop->screenCount(); ++i) {
        const QRect otherGeo = desktop->screenGeometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.y() >= screen.y() + screen.height()) {
            // other screen is completely below
            return false;
        }
    }
    // did not find a screen below our current screen, so it is the bottom most
    return true;
}

void ScreenEdges::recreateEdges()
{
    qDeleteAll(m_edges);
    m_edges.clear();
    const QRect fullArea(0, 0, displayWidth(), displayHeight());
    const QDesktopWidget *desktop = QApplication::desktop();
    for (int i=0; i<desktop->screenCount(); ++i) {
        const QRect screen = desktop->screenGeometry(i);
        if (isLeftScreen(screen, fullArea)) {
            // left most screen
            createVerticalEdge(ElectricLeft, screen, fullArea);
        }
        if (isRightScreen(screen, fullArea)) {
            // right most screen
            createVerticalEdge(ElectricRight, screen, fullArea);
        }
        if (isTopScreen(screen, fullArea)) {
            // top most screen
            createHorizontalEdge(ElectricTop, screen, fullArea);
        }
        if (isBottomScreen(screen, fullArea)) {
            // bottom most screen
            createHorizontalEdge(ElectricBottom, screen, fullArea);
        }
    }
}

void ScreenEdges::createVerticalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea)
{
    if (border != ElectricRight && border != KWin::ElectricLeft) {
        return;
    }
    int y = screen.y();
    int height = screen.height();
    const int x = (border == ElectricLeft) ? screen.x() : screen.x() + screen.width() -1;
    if (isTopScreen(screen, fullArea)) {
        // also top most screen
        height--;
        y++;
        // create top left/right edge
        const ElectricBorder edge = (border == ElectricLeft) ? ElectricTopLeft : ElectricTopRight;
        m_edges << createEdge(edge, x, screen.y(), 1, 1);
    }
    if (isBottomScreen(screen, fullArea)) {
        // also bottom most screen
        height--;
        // create bottom left/right edge
        const ElectricBorder edge = (border == ElectricLeft) ? ElectricBottomLeft : ElectricBottomRight;
        m_edges << createEdge(edge, x, screen.y() + screen.height() -1, 1, 1);
    }
    // create border
    m_edges << createEdge(border, x, y, 1, height);
}

void ScreenEdges::createHorizontalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea)
{
    if (border != ElectricTop && border != ElectricBottom) {
        return;
    }
    int x = screen.x();
    int width = screen.width();
    if (isLeftScreen(screen, fullArea)) {
        // also left most - adjust only x and width
        x++;
        width--;
    }
    if (isRightScreen(screen, fullArea)) {
        // also right most edge
        width--;
    }
    const int y = (border == ElectricTop) ? screen.y() : screen.y() + screen.height() - 1;
    m_edges << createEdge(border, x, y, width, 1);
}

WindowBasedEdge *ScreenEdges::createEdge(ElectricBorder border, int x, int y, int width, int height)
{
    WindowBasedEdge *edge = new WindowBasedEdge(this);
    edge->setBorder(border);
    edge->setGeometry(QRect(x, y, width, height));
    const ElectricBorderAction action = actionForEdge(edge);
    if (action != KWin::ElectricActionNone) {
        edge->reserve();
        edge->setAction(action);
    }
    if (isDesktopSwitching()) {
        if (edge->isCorner()) {
            edge->reserve();
        } else {
            if ((m_virtualDesktopLayout & Qt::Horizontal) && (edge->isLeft() || edge->isRight())) {
                edge->reserve();
            }
            if ((m_virtualDesktopLayout & Qt::Vertical) && (edge->isTop() || edge->isBottom())) {
                edge->reserve();
            }
        }
    }
    QHash<ElectricBorder, int>::const_iterator it = m_externalReservations.constFind(border);
    if (it != m_externalReservations.constEnd()) {
        for (int i=0; i<it.value(); ++i) {
            edge->reserve();
        }
    }
    connect(edge, SIGNAL(activated(ElectricBorder)), SIGNAL(activated(ElectricBorder)));
    return edge;
}

ElectricBorderAction ScreenEdges::actionForEdge(Edge *edge) const
{
    switch (edge->border()) {
    case ElectricTopLeft:
        return m_actionTopLeft;
    case ElectricTop:
        return m_actionTop;
    case ElectricTopRight:
        return m_actionTopRight;
    case ElectricRight:
        return m_actionRight;
    case ElectricBottomRight:
        return m_actionBottomRight;
    case ElectricBottom:
        return m_actionBottom;
    case ElectricBottomLeft:
        return m_actionBottomLeft;
    case ElectricLeft:
        return m_actionLeft;
    default:
        // fall through
        break;
    }
    return ElectricActionNone;
}

void ScreenEdges::reserveDesktopSwitching(bool isToReserve, Qt::Orientations o)
{
    if (!o)
        return;
    for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
        WindowBasedEdge *edge = *it;
        if (edge->isCorner()) {
            isToReserve ? edge->reserve() : edge->unreserve();
        } else {
            if ((m_virtualDesktopLayout & Qt::Horizontal) && (edge->isLeft() || edge->isRight())) {
                isToReserve ? edge->reserve() : edge->unreserve();
            }
            if ((m_virtualDesktopLayout & Qt::Vertical) && (edge->isTop() || edge->isBottom())) {
                isToReserve ? edge->reserve() : edge->unreserve();
            }
        }
    }
}

void ScreenEdges::reserve(ElectricBorder border)
{
    QHash<ElectricBorder, int>::iterator it = m_externalReservations.find(border);
    if (it != m_externalReservations.end()) {
        m_externalReservations.insert(border, it.value() + 1);
    }
    for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->reserve();
        }
    }
}

void ScreenEdges::unreserve(ElectricBorder border)
{
    QHash<ElectricBorder, int>::iterator it = m_externalReservations.find(border);
    if (it != m_externalReservations.end()) {
        m_externalReservations.insert(border, it.value() - 1);
    }
    for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->unreserve();
        }
    }
}

void ScreenEdges::check(const QPoint &pos, const QDateTime &now, bool forceNoPushBack)
{
    for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
        (*it)->check(pos, now, forceNoPushBack);
    }
}

bool ScreenEdges::isEntered(XEvent* e)
{
    if (e->type == EnterNotify) {
        for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
            WindowBasedEdge *edge = *it;
            if (edge->isReserved() && edge->window() == e->xcrossing.window) {
                edge->check(QPoint(e->xcrossing.x_root, e->xcrossing.y_root), QDateTime::fromMSecsSinceEpoch(e->xcrossing.time));
                return true;
            }
        }
    }
    if (e->type == ClientMessage) {
        if (e->xclient.message_type == atoms->xdnd_position) {
            for (QList<WindowBasedEdge*>::iterator it = m_edges.begin(); it != m_edges.end(); ++it) {
                WindowBasedEdge *edge = *it;
                if (edge->isReserved() && edge->window() == e->xclient.window) {
                    updateXTime();
                    edge->check(QPoint(e->xclient.data.l[2] >> 16, e->xclient.data.l[2] & 0xffff), QDateTime::fromMSecsSinceEpoch(xTime()), true);
                    return true;
                }
            }
        }
    }
    return false;
}

void ScreenEdges::ensureOnTop()
{
    Xcb::restackWindowsWithRaise(windows());
}

/*
 * NOTICE THIS IS A HACK
 * or at least a quite cumbersome way to handle conflictive electric borders
 * (like for autohiding panels or whatever)
 * the SANE solution is a central electric border daemon and a protocol
 * what this function does is to search for windows on the screen edges that are
 * * not our own screenedge
 * * either very slim
 * * or InputOnly
 * and raises them on top of everything else, including our own screen borders
 * this is typically (and for now ONLY) called when an effect input window is destroyed
 * (which raised our electric borders)
 */

void ScreenEdges::raisePanelProxies()
{
    QVector<Xcb::WindowId> ownWindows = windows();
    Xcb::Tree tree(rootWindow());
    QVector<Xcb::WindowAttributes> attributes(tree->children_len);
    QVector<Xcb::WindowGeometry> geometries(tree->children_len);

    Xcb::WindowId *windows = tree.children();
    QRect screen = QRect(0, 0, displayWidth(), displayHeight());
    QVector<Xcb::WindowId> proxies;

    int count = 0;
    for (unsigned int i = 0; i < tree->children_len; ++i) {
        if (ownWindows.contains(windows[i])) {
            // one of our screen edges
            continue;
        }
        attributes[count] = Xcb::WindowAttributes(windows[i]);
        geometries[count] = Xcb::WindowGeometry(windows[i]);
        count++;
    }

    for (int i=0; i<count; ++i) {
        Xcb::WindowAttributes attr(attributes.at(i));
        if (attr.isNull() || attr->map_state == XCB_MAP_STATE_UNMAPPED) {
            continue;
        }
        Xcb::WindowGeometry geometry(geometries.at(i));
        if (geometry.isNull()) {
            continue;
        }
        const QRect geo(geometry.rect());
        if (geo.width() < 1 || geo.height() < 1) {
            continue;
        }
        if (!(geo.width() > 1 || geo.height() > 1)) {
            continue; // random 1x1 dummy windows, all your corners are belong to us >-)
        }
        if (attr->_class != XCB_WINDOW_CLASS_INPUT_ONLY && (geo.width() > 3 && geo.height() > 3)) {
            continue;
        }
        if (geo.x() != screen.x() && geo.right() != screen.right() &&
            geo.y() != screen.y() && geo.bottom() != screen.bottom()) {
            continue;
        }
        proxies << attr.window();
    }
    Xcb::restackWindowsWithRaise(proxies);
}

QVector< xcb_window_t > ScreenEdges::windows() const
{
    QVector<xcb_window_t> wins;
    for (QList<WindowBasedEdge*>::const_iterator it = m_edges.constBegin();
            it != m_edges.constEnd();
            ++it) {
        xcb_window_t w = (*it)->window();
        if (w != XCB_WINDOW_NONE) {
            wins.append(w);
        }
    }
    return wins;
}

} //namespace
