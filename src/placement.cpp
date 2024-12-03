/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 1997-2002 Cristian Tibirna <tibirna@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placement.h"

#ifndef KCMRULES
#include "cursor.h"
#include "options.h"
#include "rules.h"
#include "virtualdesktops.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif
#endif

#include "window.h"
#include <QTextStream>
#include <QTimer>

namespace KWin
{

#ifndef KCMRULES

Placement::Placement()
{
    reinitCascading();
}

/**
 * Places the client \a c according to the workspace's layout policy
 */
bool Placement::place(Window *c, const QRectF &area)
{
    PlacementPolicy policy = c->rules()->checkPlacement(PlacementDefault);
    if (policy != PlacementDefault) {
        return place(c, area, policy);
    }

    if (c->isUtility()) {
        return placeUtility(c, area.toRect(), options->placement());
    } else if (c->isDialog()) {
        return placeDialog(c, area.toRect(), options->placement());
    } else if (c->isSplash()) {
        return placeOnMainWindow(c, area.toRect()); // on mainwindow, if any, otherwise centered
    } else if (c->isOnScreenDisplay() || c->isNotification() || c->isCriticalNotification()) {
        return placeOnScreenDisplay(c, area.toRect());
    } else if (c->isTransient() && c->hasTransientPlacementHint()) {
        return placeTransient(c);
    } else if (c->isTransient() && c->surface()) {
        return placeDialog(c, area.toRect(), options->placement());
    } else {
        return place(c, area, options->placement());
    }
}

bool Placement::place(Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement)
{
    if (policy == PlacementUnknown || policy == PlacementDefault) {
        policy = options->placement();
    }

    switch (policy) {
    case PlacementNone:
        return false;
    case PlacementRandom:
        return placeAtRandom(c, area.toRect(), nextPlacement);
    case PlacementCentered:
        return placeCentered(c, area, nextPlacement);
    case PlacementZeroCornered:
        return placeZeroCornered(c, area.toRect(), nextPlacement);
    case PlacementUnderMouse:
        return placeUnderMouse(c, area.toRect(), nextPlacement);
    case PlacementOnMainWindow:
        return placeOnMainWindow(c, area.toRect(), nextPlacement);
    case PlacementMaximizing:
        return placeMaximizing(c, area.toRect(), nextPlacement);
    default:
        return placeSmart(c, area, nextPlacement);
    }
}

/**
 * Place the client \a c according to a simply "random" placement algorithm.
 */
bool Placement::placeAtRandom(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
    }

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
    if (tx + size.width() > area.right()) {
        tx = area.right() - size.width();
        if (tx < 0) {
            tx = 0;
        }
        px = area.x();
    }
    if (ty + size.height() > area.bottom()) {
        ty = area.bottom() - size.height();
        if (ty < 0) {
            ty = 0;
        }
        py = area.y();
    }

    const QRectF placed = cascadeIfCovering(c, QRectF(QPointF(tx, ty), size), area);
    c->move(placed.topLeft());

    return true;
}

static inline bool isIrrelevant(const Window *window, const Window *regarding, VirtualDesktop *desktop)
{
    return window == regarding
        || !window->isClient()
        || !window->isShown()
        || window->isShade()
        || !window->isOnDesktop(desktop)
        || !window->isOnCurrentActivity()
        || window->isDesktop();
};

/**
 * Place the client \a c according to a really smart placement algorithm :-)
 */
bool Placement::placeSmart(Window *window, const QRectF &area, PlacementPolicy /*next*/)
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

    const QSizeF size = window->size();
    if (size.isEmpty()) {
        return false;
    }

    const int none = 0, h_wrong = -1, w_wrong = -2; // overlap types
    long int overlap, min_overlap = 0;
    int x_optimal, y_optimal;
    int possible;
    VirtualDesktop *const desktop = window->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : window->desktops().front();

    int cxl, cxr, cyt, cyb; // temp coords
    int xl, xr, yt, yb; // temp coords
    int basket; // temp holder

    // get the maximum allowed windows space
    int x = area.left();
    int y = area.top();
    x_optimal = x;
    y_optimal = y;

    // client gabarit
    int ch = std::ceil(size.height());
    int cw = std::ceil(size.width());

    // Explicitly converts those to int to avoid accidentally
    // mixing ints and qreal in the calculations below.
    int area_xr = std::floor(area.x() + area.width());
    int area_yb = std::floor(area.y() + area.height());

    bool first_pass = true; // CT lame flag. Don't like it. What else would do?

    // loop over possible positions
    do {
        // test if enough room in x and y directions
        if (y + ch > area_yb && ch < area.height()) {
            overlap = h_wrong; // this throws the algorithm to an exit
        } else if (x + cw > area_xr) {
            overlap = w_wrong;
        } else {
            overlap = none; // initialize

            cxl = x;
            cxr = x + cw;
            cyt = y;
            cyb = y + ch;
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd(); ++l) {
                auto client = *l;
                if (isIrrelevant(client, window, desktop)) {
                    continue;
                }
                xl = client->x();
                yt = client->y();
                xr = xl + client->width();
                yb = yt + client->height();

                // if windows overlap, calc the overall overlapping
                if ((cxl < xr) && (cxr > xl) && (cyt < yb) && (cyb > yt)) {
                    xl = std::max(cxl, xl);
                    xr = std::min(cxr, xr);
                    yt = std::max(cyt, yt);
                    yb = std::min(cyb, yb);
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

            possible = area_xr;
            if (possible - cw > x) {
                possible -= cw;
            }

            // compare to the position of each client on the same desk
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd(); ++l) {
                auto client = *l;
                if (isIrrelevant(client, window, desktop)) {
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
            possible = area_yb;

            if (possible - ch > y) {
                possible -= ch;
            }

            // test the position of each window on the desk
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd(); ++l) {
                auto client = *l;
                if (isIrrelevant(client, window, desktop)) {
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
    } while ((overlap != none) && (overlap != h_wrong) && (y < area_yb));

    if (ch >= area.height()) {
        y_optimal = area.top();
    }

    // place the window
    window->move(QPoint(x_optimal, y_optimal));

    return true;
}

void Placement::reinitCascading()
{
    cci.clear();
    const auto desktops = VirtualDesktopManager::self()->desktops();
    for (VirtualDesktop *desktop : desktops) {
        reinitCascading(desktop);
    }
}

void Placement::reinitCascading(VirtualDesktop *desktop)
{
    cci[desktop] = DesktopCascadingInfo{
        .pos = QPoint(-1, -1),
        .col = 0,
        .row = 0,
    };
}

QPoint Workspace::cascadeOffset(const QRectF &area) const
{
    return QPoint(area.width() / 48, area.height() / 48);
}

/**
 * Place windows in a cascading order, remembering positions for each desktop
 */
bool Placement::placeCascaded(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    Q_ASSERT(area.isValid());

    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
    }

    // CT how do I get from the 'Client' class the size that NW squarish "handle"
    const QPoint delta = workspace()->cascadeOffset(area);

    VirtualDesktop *dn = c->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : c->desktops().constLast();

    if (nextPlacement == PlacementUnknown) {
        nextPlacement = PlacementSmart;
    }

    // initialize if needed
    if (cci[dn].pos.x() < 0 || cci[dn].pos.x() < area.left() || cci[dn].pos.y() < area.top()) {
        cci[dn].pos = QPoint(area.left(), area.top());
        cci[dn].col = cci[dn].row = 0;
    }

    int xp = cci[dn].pos.x();
    int yp = cci[dn].pos.y();

    // here to touch in case people vote for resize on placement
    if ((yp + size.height()) > area.height()) {
        yp = area.top();
    }

    if ((xp + size.width()) > area.width()) {
        if (!yp) {
            return place(c, area, nextPlacement);
        } else {
            xp = area.left();
        }
    }

    // if this isn't the first window
    if (cci[dn].pos.x() != area.left() && cci[dn].pos.y() != area.top()) {
        if (xp != area.left() && yp == area.top()) {
            cci[dn].col++;
            xp = delta.x() * cci[dn].col;
        }
        if (yp != area.top() && xp == area.left()) {
            cci[dn].row++;
            yp = delta.y() * cci[dn].row;
        }

        // last resort: if still doesn't fit, smart place it
        if (((xp + size.width()) > area.width() - area.left()) || ((yp + size.height()) > area.height() - area.top())) {
            return place(c, area, nextPlacement);
        }
    }

    // place the window
    c->move(QPoint(xp, yp));

    // new position
    cci[dn].pos = QPoint(xp + delta.x(), yp + delta.y());

    return true;
}

/**
 * Place windows centered, on top of all others
 */
bool Placement::placeCentered(Window *c, const QRectF &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
    }

    const QPoint position(std::max(area.left() + (area.width() - size.width()) / 2, area.left()),
                          std::max(area.top() + (area.height() - size.height()) / 2, area.top()));

    const QRectF placed = cascadeIfCovering(c, QRectF(position, size), area);
    c->move(placed.topLeft());

    return true;
}

/**
 * Place windows in the (0,0) corner, on top of all others
 */
bool Placement::placeZeroCornered(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    Q_ASSERT(area.isValid());

    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
    }

    // get the maximum allowed windows space and desk's origin
    const QRectF placed = cascadeIfCovering(c, QRectF(area.topLeft(), size), area);
    c->move(placed.topLeft());

    return true;
}

bool Placement::placeUtility(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    // TODO kwin should try to place utility windows next to their mainwindow,
    // preferably at the right edge, and going down if there are more of them
    // if there's not enough place outside the mainwindow, it should prefer
    // top-right corner
    // use the default placement for now
    return place(c, area, PlacementDefault);
}

bool Placement::placeOnScreenDisplay(Window *c, const QRect &area)
{
    Q_ASSERT(area.isValid());

    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
    }

    // place at lower area of the screen
    const int x = area.left() + (area.width() - size.width()) / 2;
    const int y = area.top() + 2 * area.height() / 3 - size.height() / 2;

    c->move(QPoint(x, y));

    return true;
}

bool Placement::placeTransient(Window *c)
{
    c->moveResize(c->transientPlacement());
    return true;
}

bool Placement::placeDialog(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    return placeOnMainWindow(c, area, nextPlacement);
}

bool Placement::placeUnderMouse(Window *c, const QRect &area, PlacementPolicy /*next*/)
{
    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
    }

    const QPointF cursorPos = Cursors::self()->mouse()->pos();
    const QRectF centered(cursorPos.x() - size.width() / 2,
                          cursorPos.y() - size.height() / 2,
                          size.width(),
                          size.height());

    const QRectF screenArea = workspace()->clientArea(PlacementArea, c, cursorPos);
    const QRectF placed = cascadeIfCovering(c, c->keepInArea(centered, screenArea), screenArea);
    c->move(placed.topLeft());

    return true;
}

bool Placement::placeOnMainWindow(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (nextPlacement == PlacementUnknown) {
        nextPlacement = PlacementCentered;
    }
    if (nextPlacement == PlacementMaximizing) { // maximize if needed
        if (const bool placed = placeMaximizing(c, area, PlacementNone)) {
            return placed;
        }
    }

    const QSizeF size = c->size();
    if (size.isEmpty()) {
        return false;
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
                return place(c, area, PlacementCentered);
            }
        }
    }
    if (place_on == nullptr) {
        // 'mains_count' is used because it doesn't include ignored mainwindows
        if (mains_count != 1) {
            return place(c, area, PlacementCentered);
        }
        place_on = place_on2; // use the only window filtered together with 'mains_count'
    }
    if (place_on->isDesktop()) {
        return place(c, area, PlacementCentered);
    }

    QRectF geom(QPointF(0, 0), size);
    geom.moveCenter(place_on->frameGeometry().center().toPoint());

    // get area again, because the mainwindow may be on different xinerama screen
    const QRect placementArea = workspace()->clientArea(PlacementArea, c, geom.center()).toRect();
    c->move(c->keepInArea(geom, placementArea).topLeft()); // make sure it's kept inside workarea

    return true;
}

bool Placement::placeMaximizing(Window *c, const QRect &area, PlacementPolicy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (nextPlacement == PlacementUnknown) {
        nextPlacement = PlacementCentered;
    }
    if (c->isMaximizable()) {
        c->maximize(MaximizeFull);
        return true;
    } else {
        return place(c, area, nextPlacement);
    }
}

/**
 * Cascade the window until it no longer fully overlaps any other window
 */
QRectF Placement::cascadeIfCovering(Window *window, const QRectF &geometry, const QRectF &area) const
{
    const QPoint offset = workspace()->cascadeOffset(area);

    VirtualDesktop *const desktop = window->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : window->desktops().front();

    QRectF possibleGeo = geometry;
    bool noOverlap = false;

    // cascade until confirmed no total overlap or not enough space to cascade
    while (!noOverlap) {
        noOverlap = true;
        QRectF coveredArea;
        // check current position candidate for overlaps with other windows
        for (auto l = workspace()->stackingOrder().crbegin(); l != workspace()->stackingOrder().crend(); ++l) {
            auto other = *l;
            if (isIrrelevant(other, window, desktop) || !other->frameGeometry().intersects(possibleGeo)) {
                continue;
            }

            if (possibleGeo.contains(other->frameGeometry()) && !coveredArea.contains(other->frameGeometry())) {
                // placed window would completely overlap another window which is not already
                // covered by other windows: try to cascade it from the topleft of that other
                // window
                noOverlap = false;
                possibleGeo.moveTopLeft(other->pos() + offset);
                if (possibleGeo.right() > area.right() || possibleGeo.bottom() > area.bottom()) {
                    // new cascaded geometry would be out of the bounds of the placement area:
                    // abort the cascading and keep the window in the original position
                    return geometry;
                }
                break;
            }

            // keep track of the area occupied by other windows as we go from top to bottom
            // in the stacking order, so we don't need to bother trying to avoid overlap with
            // windows which are already covered up by other windows anyway
            coveredArea |= other->frameGeometry();
            if (coveredArea.contains(area)) {
                break;
            }
        }
    }

    return possibleGeo;
}

void Placement::cascadeDesktop()
{
    Workspace *ws = Workspace::self();
    reinitCascading(VirtualDesktopManager::self()->currentDesktop());
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
    const auto &windows = Workspace::self()->windows();
    for (int i = windows.size() - 1; i >= 0; i--) {
        auto window = windows.at(i);
        if (!window->isClient()) {
            continue;
        }
        if ((!window->isOnCurrentDesktop()) || (window->isMinimized()) || (window->isOnAllDesktops()) || (!window->isMovable())) {
            continue;
        }
        const QRect placementArea = workspace()->clientArea(PlacementArea, window).toRect();
        placeSmart(window, placementArea);
    }
}

#endif

const char *Placement::policyToString(PlacementPolicy policy)
{
    const char *const policies[] = {
        "NoPlacement", "Default", "XXX should never see", "Random", "Smart", "Centered",
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
        sendToOutput(moveResizeOutput()); // checks rule validity
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

    m_activeWindow->handleQuickTileShortcut(mode);
}

void Workspace::customQuickTileWindow(QuickTileMode mode)
{
    if (!m_activeWindow) {
        return;
    }

    m_activeWindow->handleCustomQuickTileShortcut(mode);
}

qreal Workspace::packPositionLeft(const Window *window, qreal oldX, bool leftEdge) const
{
    qreal newX = clientArea(MaximizeArea, window).left();
    if (oldX <= newX) { // try another Xinerama screen
        newX = clientArea(MaximizeArea,
                          window,
                          QPointF(window->frameGeometry().left() - 1, window->frameGeometry().center().y()))
                   .left();
    }
    if (oldX <= newX) {
        return oldX;
    }
    VirtualDesktop *const desktop = window->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : window->desktops().front();
    for (auto it = m_windows.constBegin(), end = m_windows.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, window, desktop)) {
            continue;
        }
        const qreal x = leftEdge ? (*it)->frameGeometry().right() : (*it)->frameGeometry().left() - 1;
        if (x > newX && x < oldX
            && !(window->frameGeometry().top() > (*it)->frameGeometry().bottom() - 1 // they overlap in Y direction
                 || window->frameGeometry().bottom() - 1 < (*it)->frameGeometry().top())) {
            newX = x;
        }
    }
    return newX;
}

qreal Workspace::packPositionRight(const Window *window, qreal oldX, bool rightEdge) const
{
    qreal newX = clientArea(MaximizeArea, window).right();
    if (oldX >= newX) { // try another Xinerama screen
        newX = clientArea(MaximizeArea,
                          window,
                          QPointF(window->frameGeometry().right(), window->frameGeometry().center().y()))
                   .right();
    }
    if (oldX >= newX) {
        return oldX;
    }
    VirtualDesktop *const desktop = window->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : window->desktops().front();
    for (auto it = m_windows.constBegin(), end = m_windows.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, window, desktop)) {
            continue;
        }
        const qreal x = rightEdge ? (*it)->frameGeometry().left() : (*it)->frameGeometry().right() + 1;

        if (x < newX && x > oldX
            && !(window->frameGeometry().top() > (*it)->frameGeometry().bottom() - 1
                 || window->frameGeometry().bottom() - 1 < (*it)->frameGeometry().top())) {
            newX = x;
        }
    }
    return newX;
}

qreal Workspace::packPositionUp(const Window *window, qreal oldY, bool topEdge) const
{
    qreal newY = clientArea(MaximizeArea, window).top();
    if (oldY <= newY) { // try another Xinerama screen
        newY = clientArea(MaximizeArea,
                          window,
                          QPointF(window->frameGeometry().center().x(), window->frameGeometry().top() - 1))
                   .top();
    }
    if (oldY <= newY) {
        return oldY;
    }
    VirtualDesktop *const desktop = window->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : window->desktops().front();
    for (auto it = m_windows.constBegin(), end = m_windows.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, window, desktop)) {
            continue;
        }
        const qreal y = topEdge ? (*it)->frameGeometry().bottom() : (*it)->frameGeometry().top() - 1;
        if (y > newY && y < oldY
            && !(window->frameGeometry().left() > (*it)->frameGeometry().right() - 1 // they overlap in X direction
                 || window->frameGeometry().right() - 1 < (*it)->frameGeometry().left())) {
            newY = y;
        }
    }
    return newY;
}

qreal Workspace::packPositionDown(const Window *window, qreal oldY, bool bottomEdge) const
{
    qreal newY = clientArea(MaximizeArea, window).bottom();
    if (oldY >= newY) { // try another Xinerama screen
        newY = clientArea(MaximizeArea,
                          window,
                          QPointF(window->frameGeometry().center().x(), window->frameGeometry().bottom()))
                   .bottom();
    }
    if (oldY >= newY) {
        return oldY;
    }
    VirtualDesktop *const desktop = window->isOnCurrentDesktop() ? VirtualDesktopManager::self()->currentDesktop() : window->desktops().front();
    for (auto it = m_windows.constBegin(), end = m_windows.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, window, desktop)) {
            continue;
        }
        const qreal y = bottomEdge ? (*it)->frameGeometry().top() : (*it)->frameGeometry().bottom() + 1;
        if (y < newY && y > oldY
            && !(window->frameGeometry().left() > (*it)->frameGeometry().right() - 1
                 || window->frameGeometry().right() - 1 < (*it)->frameGeometry().left())) {
            newY = y;
        }
    }
    return newY;
}

#endif

} // namespace
