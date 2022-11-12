/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 1997-2002 Cristian Tibirna <tibirna@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placement.h"

#ifndef KCMRULES
#include "cursor.h"
#include "options.h"
#include "rules.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "x11window.h"
#endif

#include <QTextStream>
#include <QTimer>

namespace KWin
{

#ifndef KCMRULES

Placement::Placement()
{
    reinitCascading(0);
}

/**
 * Places the client \a c according to the workspace's layout policy
 */
void Placement::place(Window *c, const QRectF &area)
{
    PlacementPolicy policy = c->rules()->checkPlacement(PlacementDefault);
    if (policy != PlacementDefault) {
        place(c, area, policy);
        return;
    }

    if (c->isUtility()) {
        placeUtility(c, area.toRect(), options->placement());
    } else if (c->isDialog()) {
        placeDialog(c, area.toRect(), options->placement());
    } else if (c->isSplash()) {
        placeOnMainWindow(c, area.toRect()); // on mainwindow, if any, otherwise centered
    } else if (c->isOnScreenDisplay() || c->isNotification() || c->isCriticalNotification()) {
        placeOnScreenDisplay(c, area.toRect());
    } else if (c->isTransient() && c->hasTransientPlacementHint()) {
        placeTransient(c);
    } else if (c->isTransient() && c->surface()) {
        placeDialog(c, area.toRect(), options->placement());
    } else {
        place(c, area, options->placement());
    }
}

void Placement::place(Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement)
{
    if (policy == PlacementUnknown || policy == PlacementDefault) {
        policy = options->placement();
    }

    switch (policy) {
    case PlacementNone:
        return;
    case PlacementRandom:
        placeAtRandom(c, area.toRect(), nextPlacement);
        break;
    case PlacementCascade:
        placeCascaded(c, area.toRect(), nextPlacement);
        break;
    case PlacementCentered:
        placeCentered(c, area, nextPlacement);
        break;
    case PlacementZeroCornered:
        placeZeroCornered(c, area.toRect(), nextPlacement);
        break;
    case PlacementUnderMouse:
        placeUnderMouse(c, area.toRect(), nextPlacement);
        break;
    case PlacementOnMainWindow:
        placeOnMainWindow(c, area.toRect(), nextPlacement);
        break;
    case PlacementMaximizing:
        placeMaximizing(c, area.toRect(), nextPlacement);
        break;
    default:
        placeSmart(c, area, nextPlacement);
    }
}

/**
 * Place the client \a c according to a simply "random" placement algorithm.
 */
void Placement::placeAtRandom(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    const int step = 24;
    static int px = step;
    static int py = 2 * step;
    int tx, ty;

    if (px < area.x()) {
        px = area.x();
    }
    if (py < area.y()) {
        py = area.y();
    }

    px += step;
    py += 2 * step;

    if (px > area.width() / 2) {
        px = area.x() + step;
    }
    if (py > area.height() / 2) {
        py = area.y() + step;
    }
    tx = px;
    ty = py;
    if (tx + c->width() > area.right()) {
        tx = area.right() - c->width();
        if (tx < 0) {
            tx = 0;
        }
        px = area.x();
    }
    if (ty + c->height() > area.bottom()) {
        ty = area.bottom() - c->height();
        if (ty < 0) {
            ty = 0;
        }
        py = area.y();
    }
    c->move(QPoint(tx, ty));
}

// TODO: one day, there'll be C++11 ...
static inline bool isIrrelevant(const Window *client, const Window *regarding, int desktop)
{
    if (!client->isClient()) {
        return true;
    }
    if (client == regarding) {
        return true;
    }
    if (!client->isShown() || client->isShade()) {
        return true;
    }
    if (!client->isOnDesktop(desktop)) {
        return true;
    }
    if (!client->isOnCurrentActivity()) {
        return true;
    }
    if (client->isDesktop()) {
        return true;
    }
    return false;
}

/**
 * Place the client \a c according to a really smart placement algorithm :-)
 */
void Placement::placeSmart(Window *c, const QRectF &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    /*
     * SmartPlacement by Cristian Tibirna (tibirna@kde.org)
     * adapted for kwm (16-19jan98) and for kwin (16Nov1999) using (with
     * permission) ideas from fvwm, authored by
     * Anthony Martin (amartin@engr.csulb.edu).
     * Xinerama supported added by Balaji Ramani (balaji@yablibli.com)
     * with ideas from xfce.
     */

    if (!c->frameGeometry().isValid()) {
        return;
    }

    const int none = 0, h_wrong = -1, w_wrong = -2; // overlap types
    long int overlap, min_overlap = 0;
    int x_optimal, y_optimal;
    int possible;
    int desktop = c->desktop() == 0 || c->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : c->desktop();

    int cxl, cxr, cyt, cyb; // temp coords
    int xl, xr, yt, yb; // temp coords
    int basket; // temp holder

    // get the maximum allowed windows space
    int x = area.left();
    int y = area.top();
    x_optimal = x;
    y_optimal = y;

    // client gabarit
    int ch = c->height() - 1;
    int cw = c->width() - 1;

    bool first_pass = true; // CT lame flag. Don't like it. What else would do?

    // loop over possible positions
    do {
        // test if enough room in x and y directions
        if (y + ch > area.bottom() && ch < area.height()) {
            overlap = h_wrong; // this throws the algorithm to an exit
        } else if (x + cw > area.right()) {
            overlap = w_wrong;
        } else {
            overlap = none; // initialize

            cxl = x;
            cxr = x + cw;
            cyt = y;
            cyb = y + ch;
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd(); ++l) {
                auto client = *l;
                if (isIrrelevant(client, c, desktop)) {
                    continue;
                }
                xl = client->x();
                yt = client->y();
                xr = xl + client->width();
                yb = yt + client->height();

                // if windows overlap, calc the overall overlapping
                if ((cxl < xr) && (cxr > xl) && (cyt < yb) && (cyb > yt)) {
                    xl = std::max(cxl, xl);
                    xr = qMin(cxr, xr);
                    yt = std::max(cyt, yt);
                    yb = qMin(cyb, yb);
                    if (client->keepAbove()) {
                        overlap += 16 * (xr - xl) * (yb - yt);
                    } else if (client->keepBelow() && !client->isDock()) { // ignore KeepBelow windows
                        overlap += 0; // for placement (see X11Window::belongsToLayer() for Dock)
                    } else {
                        overlap += (xr - xl) * (yb - yt);
                    }
                }
            }
        }

        // CT first time we get no overlap we stop.
        if (overlap == none) {
            x_optimal = x;
            y_optimal = y;
            break;
        }

        if (first_pass) {
            first_pass = false;
            min_overlap = overlap;
        }
        // CT save the best position and the minimum overlap up to now
        else if (overlap >= none && overlap < min_overlap) {
            min_overlap = overlap;
            x_optimal = x;
            y_optimal = y;
        }

        // really need to loop? test if there's any overlap
        if (overlap > none) {

            possible = area.right();
            if (possible - cw > x) {
                possible -= cw;
            }

            // compare to the position of each client on the same desk
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd(); ++l) {
                auto client = *l;
                if (isIrrelevant(client, c, desktop)) {
                    continue;
                }

                xl = client->x();
                yt = client->y();
                xr = xl + client->width();
                yb = yt + client->height();

                // if not enough room above or under the current tested client
                // determine the first non-overlapped x position
                if ((y < yb) && (yt < ch + y)) {

                    if ((xr > x) && (possible > xr)) {
                        possible = xr;
                    }

                    basket = xl - cw;
                    if ((basket > x) && (possible > basket)) {
                        possible = basket;
                    }
                }
            }
            x = possible;
        }

        // ... else ==> not enough x dimension (overlap was wrong on horizontal)
        else if (overlap == w_wrong) {
            x = area.left();
            possible = area.bottom();

            if (possible - ch > y) {
                possible -= ch;
            }

            // test the position of each window on the desk
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd(); ++l) {
                auto client = *l;
                if (isIrrelevant(client, c, desktop)) {
                    continue;
                }

                xl = client->x();
                yt = client->y();
                xr = xl + client->width();
                yb = yt + client->height();

                // if not enough room to the left or right of the current tested client
                // determine the first non-overlapped y position
                if ((yb > y) && (possible > yb)) {
                    possible = yb;
                }

                basket = yt - ch;
                if ((basket > y) && (possible > basket)) {
                    possible = basket;
                }
            }
            y = possible;
        }
    } while ((overlap != none) && (overlap != h_wrong) && (y < area.bottom()));

    if (ch >= area.height()) {
        y_optimal = area.top();
    }

    // place the window
    c->move(QPoint(x_optimal, y_optimal));
}

void Placement::reinitCascading(int desktop)
{
    // desktop == 0 - reinit all
    if (desktop == 0) {
        cci.clear();
        for (uint i = 0; i < VirtualDesktopManager::self()->count(); ++i) {
            DesktopCascadingInfo inf;
            inf.pos = QPoint(-1, -1);
            inf.col = 0;
            inf.row = 0;
            cci.append(inf);
        }
    } else {
        cci[desktop - 1].pos = QPoint(-1, -1);
        cci[desktop - 1].col = cci[desktop - 1].row = 0;
    }
}

QPoint Workspace::cascadeOffset(const Window *c) const
{
    QRect area = clientArea(PlacementArea, c, c->frameGeometry().center()).toRect();
    return QPoint(area.width() / 48, area.height() / 48);
}

/**
 * Place windows in a cascading order, remembering positions for each desktop
 */
void Placement::placeCascaded(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (!c->frameGeometry().isValid()) {
        return;
    }

    /* cascadePlacement by Cristian Tibirna (tibirna@kde.org) (30Jan98)
     */
    // work coords
    int xp, yp;

    // CT how do I get from the 'Client' class the size that NW squarish "handle"
    const QPoint delta = workspace()->cascadeOffset(c);

    const int dn = c->desktop() == 0 || c->isOnAllDesktops() ? (VirtualDesktopManager::self()->current() - 1) : (c->desktop() - 1);

    // initialize often used vars: width and height of c; we gain speed
    const int ch = c->height();
    const int cw = c->width();
    const int X = area.left();
    const int Y = area.top();
    const int H = area.height();
    const int W = area.width();

    if (nextPlacement == PlacementUnknown) {
        nextPlacement = PlacementSmart;
    }

    // initialize if needed
    if (cci[dn].pos.x() < 0 || cci[dn].pos.x() < X || cci[dn].pos.y() < Y) {
        cci[dn].pos = QPoint(X, Y);
        cci[dn].col = cci[dn].row = 0;
    }

    xp = cci[dn].pos.x();
    yp = cci[dn].pos.y();

    // here to touch in case people vote for resize on placement
    if ((yp + ch) > H) {
        yp = Y;
    }

    if ((xp + cw) > W) {
        if (!yp) {
            place(c, area, nextPlacement);
            return;
        } else {
            xp = X;
        }
    }

    // if this isn't the first window
    if (cci[dn].pos.x() != X && cci[dn].pos.y() != Y) {
        /* The following statements cause an internal compiler error with
         * egcs-2.91.66 on SuSE Linux 6.3. The equivalent forms compile fine.
         * 22-Dec-1999 CS
         *
         * if (xp != X && yp == Y) xp = delta.x() * (++(cci[dn].col));
         * if (yp != Y && xp == X) yp = delta.y() * (++(cci[dn].row));
         */
        if (xp != X && yp == Y) {
            ++(cci[dn].col);
            xp = delta.x() * cci[dn].col;
        }
        if (yp != Y && xp == X) {
            ++(cci[dn].row);
            yp = delta.y() * cci[dn].row;
        }

        // last resort: if still doesn't fit, smart place it
        if (((xp + cw) > W - X) || ((yp + ch) > H - Y)) {
            place(c, area, nextPlacement);
            return;
        }
    }

    // place the window
    c->move(QPoint(xp, yp));

    // new position
    cci[dn].pos = QPoint(xp + delta.x(), yp + delta.y());
}

/**
 * Place windows centered, on top of all others
 */
void Placement::placeCentered(Window *c, const QRectF &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    if (!c->frameGeometry().isValid()) {
        return;
    }

    const int xp = area.left() + (area.width() - c->width()) / 2;
    const int yp = area.top() + (area.height() - c->height()) / 2;

    // place the window
    c->move(QPoint(xp, yp));
}

/**
 * Place windows in the (0,0) corner, on top of all others
 */
void Placement::placeZeroCornered(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    // get the maximum allowed windows space and desk's origin
    c->move(area.topLeft());
}

void Placement::placeUtility(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    // TODO kwin should try to place utility windows next to their mainwindow,
    // preferably at the right edge, and going down if there are more of them
    // if there's not enough place outside the mainwindow, it should prefer
    // top-right corner
    // use the default placement for now
    place(c, area, PlacementDefault);
}

void Placement::placeOnScreenDisplay(Window *c, const QRect &area)
{
    Q_ASSERT(area.isValid());

    // place at lower area of the screen
    const int x = area.left() + (area.width() - c->width()) / 2;
    const int y = area.top() + 2 * area.height() / 3 - c->height() / 2;

    c->move(QPoint(x, y));
}

void Placement::placeTransient(Window *c)
{
    const auto parent = c->transientFor();
    const QRectF screen = Workspace::self()->clientArea(parent->isFullScreen() ? FullScreenArea : PlacementArea, parent);
    c->moveResize(c->transientPlacement(screen));

    // Potentially a client could set no constraint adjustments
    // and we'll be offscreen.

    // The spec implies we should place window the offscreen. However,
    // practically Qt doesn't set any constraint adjustments yet so we can't.
    // Also kwin generally doesn't let clients do what they want
    if (!screen.contains(c->moveResizeGeometry().toAlignedRect())) {
        c->keepInArea(screen);
    }
}

void Placement::placeDialog(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    placeOnMainWindow(c, area, nextPlacement);
}

void Placement::placeUnderMouse(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    QRectF geom = c->frameGeometry();
    geom.moveCenter(Cursors::self()->mouse()->pos());
    c->move(geom.topLeft().toPoint());
    c->keepInArea(area); // make sure it's kept inside workarea
}

void Placement::placeOnMainWindow(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (nextPlacement == PlacementUnknown) {
        nextPlacement = PlacementCentered;
    }
    if (nextPlacement == PlacementMaximizing) { // maximize if needed
        placeMaximizing(c, area, PlacementNone);
    }
    auto mainwindows = c->mainWindows();
    Window *place_on = nullptr;
    Window *place_on2 = nullptr;
    int mains_count = 0;
    for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd(); ++it) {
        if (mainwindows.count() > 1 && (*it)->isSpecialWindow()) {
            continue; // don't consider toolbars etc when placing
        }
        ++mains_count;
        place_on2 = *it;
        if ((*it)->isOnCurrentDesktop()) {
            if (place_on == nullptr) {
                place_on = *it;
            } else {
                // two or more on current desktop -> center
                // That's the default at least. However, with maximizing placement
                // policy as the default, the dialog should be either maximized or
                // made as large as its maximum size and then placed centered.
                // So the nextPlacement argument allows chaining. In this case, nextPlacement
                // is Maximizing and it will call placeCentered().
                place(c, area, PlacementCentered);
                return;
            }
        }
    }
    if (place_on == nullptr) {
        // 'mains_count' is used because it doesn't include ignored mainwindows
        if (mains_count != 1) {
            place(c, area, PlacementCentered);
            return;
        }
        place_on = place_on2; // use the only window filtered together with 'mains_count'
    }
    if (place_on->isDesktop()) {
        place(c, area, PlacementCentered);
        return;
    }
    QRect geom = c->frameGeometry().toRect();
    geom.moveCenter(place_on->frameGeometry().center().toPoint());
    c->move(geom.topLeft());
    // get area again, because the mainwindow may be on different xinerama screen
    const QRect placementArea = workspace()->clientArea(PlacementArea, c).toRect();
    c->keepInArea(placementArea); // make sure it's kept inside workarea
}

void Placement::placeMaximizing(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (nextPlacement == PlacementUnknown) {
        nextPlacement = PlacementSmart;
    }
    if (c->isMaximizable() && c->maxSize().width() >= area.width() && c->maxSize().height() >= area.height()) {
        if (workspace()->clientArea(MaximizeArea, c) == area) {
            c->maximize(MaximizeFull);
        } else { // if the geometry doesn't match default maximize area (xinerama case?),
            // it's probably better to use the given area
            c->moveResize(area);
        }
    } else {
        c->moveResize(c->resizeWithChecks(c->moveResizeGeometry(), c->maxSize().boundedTo(area.size())));
        place(c, area, nextPlacement);
    }
}

void Placement::cascadeDesktop()
{
    Workspace *ws = Workspace::self();
    const int desktop = VirtualDesktopManager::self()->current();
    reinitCascading(desktop);
    const auto stackingOrder = ws->stackingOrder();
    for (Window *window : stackingOrder) {
        if (!window->isClient() || (!window->isOnCurrentDesktop()) || (window->isMinimized()) || (window->isOnAllDesktops()) || (!window->isMovable())) {
            continue;
        }
        const QRect placementArea = workspace()->clientArea(PlacementArea, window).toRect();
        placeCascaded(window, placementArea);
    }
}

void Placement::unclutterDesktop()
{
    const auto &clients = Workspace::self()->allClientList();
    for (int i = clients.size() - 1; i >= 0; i--) {
        auto client = clients.at(i);
        if ((!client->isOnCurrentDesktop()) || (client->isMinimized()) || (client->isOnAllDesktops()) || (!client->isMovable())) {
            continue;
        }
        const QRect placementArea = workspace()->clientArea(PlacementArea, client).toRect();
        placeSmart(client, placementArea);
    }
}

#endif

const char *Placement::policyToString(PlacementPolicy policy)
{
    const char *const policies[] = {
        "NoPlacement", "Default", "XXX should never see", "Random", "Smart", "Cascade", "Centered",
        "ZeroCornered", "UnderMouse", "OnMainWindow", "Maximizing"};
    Q_ASSERT(policy < int(sizeof(policies) / sizeof(policies[0])));
    return policies[policy];
}

#ifndef KCMRULES

// ********************
// Workspace
// ********************

void Window::packTo(qreal left, qreal top)
{
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;

    const Output *oldOutput = moveResizeOutput();
    move(QPoint(left, top));
    if (moveResizeOutput() != oldOutput) {
        workspace()->sendWindowToOutput(this, moveResizeOutput()); // checks rule validity
        if (requestedMaximizeMode() != MaximizeRestore) {
            checkWorkspacePosition();
        }
    }
}

/**
 * Moves active window left until in bumps into another window or workarea edge.
 */
void Workspace::slotWindowMoveLeft()
{
    if (m_activeWindow && m_activeWindow->isMovable()) {
        const QRectF geometry = m_activeWindow->moveResizeGeometry();
        m_activeWindow->packTo(packPositionLeft(m_activeWindow, geometry.left(), true),
                               geometry.y());
    }
}

void Workspace::slotWindowMoveRight()
{
    if (m_activeWindow && m_activeWindow->isMovable()) {
        const QRectF geometry = m_activeWindow->moveResizeGeometry();
        m_activeWindow->packTo(packPositionRight(m_activeWindow, geometry.right(), true) - geometry.width(),
                               geometry.y());
    }
}

void Workspace::slotWindowMoveUp()
{
    if (m_activeWindow && m_activeWindow->isMovable()) {
        const QRectF geometry = m_activeWindow->moveResizeGeometry();
        m_activeWindow->packTo(geometry.x(),
                               packPositionUp(m_activeWindow, geometry.top(), true));
    }
}

void Workspace::slotWindowMoveDown()
{
    if (m_activeWindow && m_activeWindow->isMovable()) {
        const QRectF geometry = m_activeWindow->moveResizeGeometry();
        m_activeWindow->packTo(geometry.x(),
                               packPositionDown(m_activeWindow, geometry.bottom(), true) - geometry.height());
    }
}

/** Moves the active window to the center of the screen. */
void Workspace::slotWindowCenter()
{
    if (m_activeWindow && m_activeWindow->isMovable()) {
        const QRectF geometry = m_activeWindow->moveResizeGeometry();
        QPointF center = clientArea(MaximizeArea, m_activeWindow).center();
        m_activeWindow->packTo(center.x() - (geometry.width() / 2),
                               center.y() - (geometry.height() / 2));
    }
}

void Workspace::slotWindowExpandHorizontal()
{
    if (m_activeWindow) {
        m_activeWindow->growHorizontal();
    }
}

void Window::growHorizontal()
{
    if (!isResizable() || isShade()) {
        return;
    }
    QRectF geom = moveResizeGeometry();
    geom.setRight(workspace()->packPositionRight(this, geom.right(), true));
    QSizeF adjsize = constrainFrameSize(geom.size(), SizeModeFixedW);
    if (moveResizeGeometry().size() == adjsize && geom.size() != adjsize && resizeIncrements().width() > 1) { // take care of size increments
        qreal newright = workspace()->packPositionRight(this, geom.right() + resizeIncrements().width() - 1, true);
        // check that it hasn't grown outside of the area, due to size increments
        // TODO this may be wrong?
        if (workspace()->clientArea(MovementArea,
                                    this,
                                    QPoint((x() + newright) / 2, moveResizeGeometry().center().y()))
                .right()
            >= newright) {
            geom.setRight(newright);
        }
    }
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedW));
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedH));
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
    moveResize(geom);
}

void Workspace::slotWindowShrinkHorizontal()
{
    if (m_activeWindow) {
        m_activeWindow->shrinkHorizontal();
    }
}

void Window::shrinkHorizontal()
{
    if (!isResizable() || isShade()) {
        return;
    }
    QRectF geom = moveResizeGeometry();
    geom.setRight(workspace()->packPositionLeft(this, geom.right(), false) + 1);
    if (geom.width() <= 1) {
        return;
    }
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedW));
    if (geom.width() > 20) {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
        moveResize(geom);
    }
}

void Workspace::slotWindowExpandVertical()
{
    if (m_activeWindow) {
        m_activeWindow->growVertical();
    }
}

void Window::growVertical()
{
    if (!isResizable() || isShade()) {
        return;
    }
    QRectF geom = moveResizeGeometry();
    geom.setBottom(workspace()->packPositionDown(this, geom.bottom(), true));
    QSizeF adjsize = constrainFrameSize(geom.size(), SizeModeFixedH);
    if (moveResizeGeometry().size() == adjsize && geom.size() != adjsize && resizeIncrements().height() > 1) { // take care of size increments
        qreal newbottom = workspace()->packPositionDown(this, geom.bottom() + resizeIncrements().height(), true);
        // check that it hasn't grown outside of the area, due to size increments
        if (workspace()->clientArea(MovementArea,
                                    this,
                                    QPoint(moveResizeGeometry().center().x(), (y() + newbottom) / 2))
                .bottom()
            >= newbottom) {
            geom.setBottom(newbottom);
        }
    }
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedH));
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
    moveResize(geom);
}

void Workspace::slotWindowShrinkVertical()
{
    if (m_activeWindow) {
        m_activeWindow->shrinkVertical();
    }
}

void Window::shrinkVertical()
{
    if (!isResizable() || isShade()) {
        return;
    }
    QRectF geom = moveResizeGeometry();
    geom.setBottom(workspace()->packPositionUp(this, geom.bottom(), false) + 1);
    if (geom.height() <= 1) {
        return;
    }
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedH));
    if (geom.height() > 20) {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
        moveResize(geom);
    }
}

void Workspace::quickTileWindow(QuickTileMode mode)
{
    if (!m_activeWindow) {
        return;
    }

    // If the user invokes two of these commands in a one second period, try to
    // combine them together to enable easy and intuitive corner tiling
#define FLAG(name) QuickTileMode(QuickTileFlag::name)
    if (!m_quickTileCombineTimer->isActive()) {
        m_quickTileCombineTimer->start(1000);
        m_lastTilingMode = mode;
    } else {
        if (
            ((m_lastTilingMode == FLAG(Left) || m_lastTilingMode == FLAG(Right)) && (mode == FLAG(Top) || mode == FLAG(Bottom)))
            || ((m_lastTilingMode == FLAG(Top) || m_lastTilingMode == FLAG(Bottom)) && (mode == FLAG(Left) || mode == FLAG(Right)))
#undef FLAG
        ) {
            mode |= m_lastTilingMode;
        }
        m_quickTileCombineTimer->stop();
    }

    m_activeWindow->setQuickTileMode(mode, true);
}

qreal Workspace::packPositionLeft(const Window *client, qreal oldX, bool leftEdge) const
{
    qreal newX = clientArea(MaximizeArea, client).left();
    if (oldX <= newX) { // try another Xinerama screen
        newX = clientArea(MaximizeArea,
                          client,
                          QPointF(client->frameGeometry().left() - 1, client->frameGeometry().center().y()))
                   .left();
    }
    if (oldX <= newX) {
        return oldX;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const qreal x = leftEdge ? (*it)->frameGeometry().right() : (*it)->frameGeometry().left() - 1;
        if (x > newX && x < oldX
            && !(client->frameGeometry().top() > (*it)->frameGeometry().bottom() - 1 // they overlap in Y direction
                 || client->frameGeometry().bottom() - 1 < (*it)->frameGeometry().top())) {
            newX = x;
        }
    }
    return newX;
}

qreal Workspace::packPositionRight(const Window *client, qreal oldX, bool rightEdge) const
{
    qreal newX = clientArea(MaximizeArea, client).right();
    if (oldX >= newX) { // try another Xinerama screen
        newX = clientArea(MaximizeArea,
                          client,
                          QPointF(client->frameGeometry().right(), client->frameGeometry().center().y()))
                   .right();
    }
    if (oldX >= newX) {
        return oldX;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const qreal x = rightEdge ? (*it)->frameGeometry().left() : (*it)->frameGeometry().right() + 1;

        if (x < newX && x > oldX
            && !(client->frameGeometry().top() > (*it)->frameGeometry().bottom() - 1
                 || client->frameGeometry().bottom() - 1 < (*it)->frameGeometry().top())) {
            newX = x;
        }
    }
    return newX;
}

qreal Workspace::packPositionUp(const Window *client, qreal oldY, bool topEdge) const
{
    qreal newY = clientArea(MaximizeArea, client).top();
    if (oldY <= newY) { // try another Xinerama screen
        newY = clientArea(MaximizeArea,
                          client,
                          QPointF(client->frameGeometry().center().x(), client->frameGeometry().top() - 1))
                   .top();
    }
    if (oldY <= newY) {
        return oldY;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const qreal y = topEdge ? (*it)->frameGeometry().bottom() : (*it)->frameGeometry().top() - 1;
        if (y > newY && y < oldY
            && !(client->frameGeometry().left() > (*it)->frameGeometry().right() - 1 // they overlap in X direction
                 || client->frameGeometry().right() - 1 < (*it)->frameGeometry().left())) {
            newY = y;
        }
    }
    return newY;
}

qreal Workspace::packPositionDown(const Window *client, qreal oldY, bool bottomEdge) const
{
    qreal newY = clientArea(MaximizeArea, client).bottom();
    if (oldY >= newY) { // try another Xinerama screen
        newY = clientArea(MaximizeArea,
                          client,
                          QPointF(client->frameGeometry().center().x(), client->frameGeometry().bottom()))
                   .bottom();
    }
    if (oldY >= newY) {
        return oldY;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const qreal y = bottomEdge ? (*it)->frameGeometry().top() : (*it)->frameGeometry().bottom() + 1;
        if (y < newY && y > oldY
            && !(client->frameGeometry().left() > (*it)->frameGeometry().right() - 1
                 || client->frameGeometry().right() - 1 < (*it)->frameGeometry().left())) {
            newY = y;
        }
    }
    return newY;
}

#endif

} // namespace
