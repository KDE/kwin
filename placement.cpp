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
#include "workspace.h"
#include "x11client.h"
#include "cursor.h"
#include "options.h"
#include "rules.h"
#include "screens.h"
#endif

#include <QRect>
#include <QTextStream>
#include <QTimer>

namespace KWin
{

#ifndef KCMRULES

KWIN_SINGLETON_FACTORY(Placement)

Placement::Placement(QObject*)
{
    reinitCascading(0);
}

Placement::~Placement()
{
    s_self = nullptr;
}

/**
 * Places the client \a c according to the workspace's layout policy
 */
void Placement::place(AbstractClient *c, const QRect &area)
{
    Policy policy = c->rules()->checkPlacement(Default);
    if (policy != Default) {
        place(c, area, policy);
        return;
    }

    if (c->isUtility())
        placeUtility(c, area, options->placement());
    else if (c->isDialog())
        placeDialog(c, area, options->placement());
    else if (c->isSplash())
        placeOnMainWindow(c, area);   // on mainwindow, if any, otherwise centered
    else if (c->isOnScreenDisplay() || c->isNotification() || c->isCriticalNotification())
        placeOnScreenDisplay(c, area);
    else if (c->isTransient() && c->hasTransientPlacementHint())
        placeTransient(c);
    else if (c->isTransient() && c->surface())
        placeDialog(c, area, options->placement());
    else
        place(c, area, options->placement());
}

void Placement::place(AbstractClient *c, const QRect &area, Policy policy, Policy nextPlacement)
{
    if (policy == Unknown)
        policy = Default;
    if (policy == Default)
        policy = options->placement();
    if (policy == NoPlacement)
        return;
    else if (policy == Random)
        placeAtRandom(c, area, nextPlacement);
    else if (policy == Cascade)
        placeCascaded(c, area, nextPlacement);
    else if (policy == Centered)
        placeCentered(c, area, nextPlacement);
    else if (policy == ZeroCornered)
        placeZeroCornered(c, area, nextPlacement);
    else if (policy == UnderMouse)
        placeUnderMouse(c, area, nextPlacement);
    else if (policy == OnMainWindow)
        placeOnMainWindow(c, area, nextPlacement);
    else if (policy == Maximizing)
        placeMaximizing(c, area, nextPlacement);
    else
        placeSmart(c, area, nextPlacement);

    if (options->borderSnapZone()) {
        // snap to titlebar / snap to window borders on inner screen edges
        const QRect geo(c->frameGeometry());
        QPoint corner = geo.topLeft();
        const QMargins frameMargins = c->frameMargins();
        AbstractClient::Position titlePos = c->titlebarPosition();

        const QRect fullRect = workspace()->clientArea(FullArea, c);
        if (!(c->maximizeMode() & MaximizeHorizontal)) {
            if (titlePos != AbstractClient::PositionRight && geo.right() == fullRect.right()) {
                corner.rx() += frameMargins.right();
            }
            if (titlePos != AbstractClient::PositionLeft && geo.left() == fullRect.left()) {
                corner.rx() -= frameMargins.left();
            }
        }
        if (!(c->maximizeMode() & MaximizeVertical)) {
            if (titlePos != AbstractClient::PositionBottom && geo.bottom() == fullRect.bottom()) {
                corner.ry() += frameMargins.bottom();
            }
            if (titlePos != AbstractClient::PositionTop && geo.top() == fullRect.top()) {
                corner.ry() -= frameMargins.top();
            }
        }
        c->move(corner);
    }
}

/**
 * Place the client \a c according to a simply "random" placement algorithm.
 */
void Placement::placeAtRandom(AbstractClient* c, const QRect& area, Policy /*next*/)
{
    Q_ASSERT(area.isValid());

    const int step  = 24;
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
        if (tx < 0)
            tx = 0;
        px = area.x();
    }
    if (ty + c->height() > area.bottom()) {
        ty = area.bottom() - c->height();
        if (ty < 0)
            ty = 0;
        py = area.y();
    }
    c->move(tx, ty);
}

// TODO: one day, there'll be C++11 ...
static inline bool isIrrelevant(const AbstractClient *client, const AbstractClient *regarding, int desktop)
{
    if (!client)
        return true;
    if (client == regarding)
        return true;
    if (!client->isShown(false))
        return true;
    if (!client->isOnDesktop(desktop))
        return true;
    if (!client->isOnCurrentActivity())
        return true;
    if (client->isDesktop())
        return true;
    return false;
}

/**
 * Place the client \a c according to a really smart placement algorithm :-)
 */
void Placement::placeSmart(AbstractClient* c, const QRect& area, Policy /*next*/)
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

    int cxl, cxr, cyt, cyb;     //temp coords
    int  xl, xr, yt, yb;     //temp coords
    int basket;                 //temp holder

    // get the maximum allowed windows space
    int x = area.left();
    int y = area.top();
    x_optimal = x; y_optimal = y;

    //client gabarit
    int ch = c->height() - 1;
    int cw = c->width()  - 1;

    bool first_pass = true; //CT lame flag. Don't like it. What else would do?

    //loop over possible positions
    do {
        //test if enough room in x and y directions
        if (y + ch > area.bottom() && ch < area.height()) {
            overlap = h_wrong; // this throws the algorithm to an exit
        } else if (x + cw > area.right()) {
            overlap = w_wrong;
        } else {
            overlap = none; //initialize

            cxl = x; cxr = x + cw;
            cyt = y; cyb = y + ch;
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd() ; ++l) {
                AbstractClient *client = qobject_cast<AbstractClient*>(*l);
                if (isIrrelevant(client, c, desktop)) {
                    continue;
                }
                xl = client->x();          yt = client->y();
                xr = xl + client->width(); yb = yt + client->height();

                //if windows overlap, calc the overall overlapping
                if ((cxl < xr) && (cxr > xl) &&
                        (cyt < yb) && (cyb > yt)) {
                    xl = qMax(cxl, xl); xr = qMin(cxr, xr);
                    yt = qMax(cyt, yt); yb = qMin(cyb, yb);
                    if (client->keepAbove())
                        overlap += 16 * (xr - xl) * (yb - yt);
                    else if (client->keepBelow() && !client->isDock()) // ignore KeepBelow windows
                        overlap += 0; // for placement (see X11Client::belongsToLayer() for Dock)
                    else
                        overlap += (xr - xl) * (yb - yt);
                }
            }
        }

        //CT first time we get no overlap we stop.
        if (overlap == none) {
            x_optimal = x;
            y_optimal = y;
            break;
        }

        if (first_pass) {
            first_pass = false;
            min_overlap = overlap;
        }
        //CT save the best position and the minimum overlap up to now
        else if (overlap >= none && overlap < min_overlap) {
            min_overlap = overlap;
            x_optimal = x;
            y_optimal = y;
        }

        // really need to loop? test if there's any overlap
        if (overlap > none) {

            possible = area.right();
            if (possible - cw > x) possible -= cw;

            // compare to the position of each client on the same desk
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd() ; ++l) {
                AbstractClient *client = qobject_cast<AbstractClient*>(*l);
                if (isIrrelevant(client, c, desktop)) {
                    continue;
                }

                xl = client->x();          yt = client->y();
                xr = xl + client->width(); yb = yt + client->height();

                // if not enough room above or under the current tested client
                // determine the first non-overlapped x position
                if ((y < yb) && (yt < ch + y)) {

                    if ((xr > x) && (possible > xr)) possible = xr;

                    basket = xl - cw;
                    if ((basket > x) && (possible > basket)) possible = basket;
                }
            }
            x = possible;
        }

        // ... else ==> not enough x dimension (overlap was wrong on horizontal)
        else if (overlap == w_wrong) {
            x = area.left();
            possible = area.bottom();

            if (possible - ch > y) possible -= ch;

            //test the position of each window on the desk
            for (auto l = workspace()->stackingOrder().constBegin(); l != workspace()->stackingOrder().constEnd() ; ++l) {
                AbstractClient *client = qobject_cast<AbstractClient*>(*l);
                if (isIrrelevant(client, c, desktop)) {
                    continue;
                }

                xl = client->x();          yt = client->y();
                xr = xl + client->width(); yb = yt + client->height();

                // if not enough room to the left or right of the current tested client
                // determine the first non-overlapped y position
                if ((yb > y) && (possible > yb)) possible = yb;

                basket = yt - ch;
                if ((basket > y) && (possible > basket)) possible = basket;
            }
            y = possible;
        }
    } while ((overlap != none) && (overlap != h_wrong) && (y < area.bottom()));

    if (ch >= area.height()) {
        y_optimal = area.top();
    }

    // place the window
    c->move(x_optimal, y_optimal);

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

QPoint Workspace::cascadeOffset(const AbstractClient *c) const
{
    QRect area = clientArea(PlacementArea, c->frameGeometry().center(), c->desktop());
    return QPoint(area.width()/48, area.height()/48);
}

/**
 * Place windows in a cascading order, remembering positions for each desktop
 */
void Placement::placeCascaded(AbstractClient *c, const QRect &area, Policy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (!c->frameGeometry().isValid()) {
        return;
    }

    /* cascadePlacement by Cristian Tibirna (tibirna@kde.org) (30Jan98)
     */
    // work coords
    int xp, yp;

    //CT how do I get from the 'Client' class the size that NW squarish "handle"
    const QPoint delta = workspace()->cascadeOffset(c);

    const int dn = c->desktop() == 0 || c->isOnAllDesktops() ? (VirtualDesktopManager::self()->current() - 1) : (c->desktop() - 1);

    // initialize often used vars: width and height of c; we gain speed
    const int ch = c->height();
    const int cw = c->width();
    const int X = area.left();
    const int Y = area.top();
    const int H = area.height();
    const int W = area.width();

    if (nextPlacement == Unknown)
        nextPlacement = Smart;

    //initialize if needed
    if (cci[dn].pos.x() < 0 || cci[dn].pos.x() < X || cci[dn].pos.y() < Y) {
        cci[dn].pos = QPoint(X, Y);
        cci[dn].col = cci[dn].row = 0;
    }


    xp = cci[dn].pos.x();
    yp = cci[dn].pos.y();

    //here to touch in case people vote for resize on placement
    if ((yp + ch) > H) yp = Y;

    if ((xp + cw) > W) {
        if (!yp) {
            place(c, area, nextPlacement);
            return;
        } else xp = X;
    }

    //if this isn't the first window
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
void Placement::placeCentered(AbstractClient* c, const QRect& area, Policy /*next*/)
{
    Q_ASSERT(area.isValid());

    const int xp = area.left() + (area.width() - c->width()) / 2;
    const int yp = area.top() + (area.height() - c->height()) / 2;

    // place the window
    c->move(QPoint(xp, yp));
}

/**
 * Place windows in the (0,0) corner, on top of all others
 */
void Placement::placeZeroCornered(AbstractClient* c, const QRect& area, Policy /*next*/)
{
    Q_ASSERT(area.isValid());

    // get the maximum allowed windows space and desk's origin
    c->move(area.topLeft());
}

void Placement::placeUtility(AbstractClient *c, const QRect &area, Policy /*next*/)
{
// TODO kwin should try to place utility windows next to their mainwindow,
// preferably at the right edge, and going down if there are more of them
// if there's not enough place outside the mainwindow, it should prefer
// top-right corner
    // use the default placement for now
    place(c, area, Default);
}

void Placement::placeOnScreenDisplay(AbstractClient *c, const QRect &area)
{
    Q_ASSERT(area.isValid());

    // place at lower area of the screen
    const int x = area.left() + (area.width() -  c->width())  / 2;
    const int y = area.top() + 2 * area.height() / 3 - c->height() / 2;

    c->move(QPoint(x, y));
}

void Placement::placeTransient(AbstractClient *c)
{
    const auto parent = c->transientFor();
    const QRect screen =  Workspace::self()->clientArea(parent->isFullScreen() ? FullScreenArea : PlacementArea, parent);
    const QRect popupGeometry = c->transientPlacement(screen);
    c->setFrameGeometry(popupGeometry);


    // Potentially a client could set no constraint adjustments
    // and we'll be offscreen.

    // The spec implies we should place window the offscreen. However,
    // practically Qt doesn't set any constraint adjustments yet so we can't.
    // Also kwin generally doesn't let clients do what they want
    if (!screen.contains(c->frameGeometry())) {
        c->keepInArea(screen);
    }
}

void Placement::placeDialog(AbstractClient *c, const QRect &area, Policy nextPlacement)
{
    placeOnMainWindow(c, area, nextPlacement);
}

void Placement::placeUnderMouse(AbstractClient *c, const QRect &area, Policy /*next*/)
{
    Q_ASSERT(area.isValid());

    QRect geom = c->frameGeometry();
    geom.moveCenter(Cursors::self()->mouse()->pos());
    c->move(geom.topLeft());
    c->keepInArea(area);   // make sure it's kept inside workarea
}

void Placement::placeOnMainWindow(AbstractClient *c, const QRect &area, Policy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (nextPlacement == Unknown)
        nextPlacement = Centered;
    if (nextPlacement == Maximizing)   // maximize if needed
        placeMaximizing(c, area, NoPlacement);
    auto mainwindows = c->mainClients();
    AbstractClient* place_on = nullptr;
    AbstractClient* place_on2 = nullptr;
    int mains_count = 0;
    for (auto it = mainwindows.constBegin();
            it != mainwindows.constEnd();
            ++it) {
        if (mainwindows.count() > 1 && (*it)->isSpecialWindow())
            continue; // don't consider toolbars etc when placing
        ++mains_count;
        place_on2 = *it;
        if ((*it)->isOnCurrentDesktop()) {
            if (place_on == nullptr)
                place_on = *it;
            else {
                // two or more on current desktop -> center
                // That's the default at least. However, with maximizing placement
                // policy as the default, the dialog should be either maximized or
                // made as large as its maximum size and then placed centered.
                // So the nextPlacement argument allows chaining. In this case, nextPlacement
                // is Maximizing and it will call placeCentered().
                place(c, area, Centered);
                return;
            }
        }
    }
    if (place_on == nullptr) {
        // 'mains_count' is used because it doesn't include ignored mainwindows
        if (mains_count != 1) {
            place(c, area, Centered);
            return;
        }
        place_on = place_on2; // use the only window filtered together with 'mains_count'
    }
    if (place_on->isDesktop()) {
        place(c, area, Centered);
        return;
    }
    QRect geom = c->frameGeometry();
    geom.moveCenter(place_on->frameGeometry().center());
    c->move(geom.topLeft());
    // get area again, because the mainwindow may be on different xinerama screen
    const QRect placementArea = workspace()->clientArea(PlacementArea, c);
    c->keepInArea(placementArea);   // make sure it's kept inside workarea
}

void Placement::placeMaximizing(AbstractClient *c, const QRect &area, Policy nextPlacement)
{
    Q_ASSERT(area.isValid());

    if (nextPlacement == Unknown)
        nextPlacement = Smart;
    if (c->isMaximizable() && c->maxSize().width() >= area.width() && c->maxSize().height() >= area.height()) {
        if (workspace()->clientArea(MaximizeArea, c) == area)
            c->maximize(MaximizeFull);
        else { // if the geometry doesn't match default maximize area (xinerama case?),
            // it's probably better to use the given area
            c->setFrameGeometry(area);
        }
    } else {
        c->resizeWithChecks(c->maxSize().boundedTo(area.size()));
        place(c, area, nextPlacement);
    }
}

void Placement::cascadeDesktop()
{
    Workspace *ws = Workspace::self();
    const int desktop = VirtualDesktopManager::self()->current();
    reinitCascading(desktop);
    foreach (Toplevel *toplevel, ws->stackingOrder()) {
        auto client = qobject_cast<AbstractClient*>(toplevel);
        if (!client ||
                (!client->isOnCurrentDesktop()) ||
                (client->isMinimized())         ||
                (client->isOnAllDesktops())     ||
                (!client->isMovable()))
            continue;
        const QRect placementArea = workspace()->clientArea(PlacementArea, client);
        placeCascaded(client, placementArea);
    }
}

void Placement::unclutterDesktop()
{
    const auto &clients = Workspace::self()->allClientList();
    for (int i = clients.size() - 1; i >= 0; i--) {
        auto client = clients.at(i);
        if ((!client->isOnCurrentDesktop()) ||
                (client->isMinimized())     ||
                (client->isOnAllDesktops()) ||
                (!client->isMovable()))
            continue;
        const QRect placementArea = workspace()->clientArea(PlacementArea, client);
        placeSmart(client, placementArea);
    }
}

#endif


Placement::Policy Placement::policyFromString(const QString& policy, bool no_special)
{
    if (policy == QStringLiteral("NoPlacement"))
        return NoPlacement;
    else if (policy == QStringLiteral("Default") && !no_special)
        return Default;
    else if (policy == QStringLiteral("Random"))
        return Random;
    else if (policy == QStringLiteral("Cascade"))
        return Cascade;
    else if (policy == QStringLiteral("Centered"))
        return Centered;
    else if (policy == QStringLiteral("ZeroCornered"))
        return ZeroCornered;
    else if (policy == QStringLiteral("UnderMouse"))
        return UnderMouse;
    else if (policy == QStringLiteral("OnMainWindow") && !no_special)
        return OnMainWindow;
    else if (policy == QStringLiteral("Maximizing"))
        return Maximizing;
    else
        return Smart;
}

const char* Placement::policyToString(Policy policy)
{
    const char* const policies[] = {
        "NoPlacement", "Default", "XXX should never see", "Random", "Smart", "Cascade", "Centered",
        "ZeroCornered", "UnderMouse", "OnMainWindow", "Maximizing"
    };
    Q_ASSERT(policy < int(sizeof(policies) / sizeof(policies[ 0 ])));
    return policies[ policy ];
}


#ifndef KCMRULES

// ********************
// Workspace
// ********************

void AbstractClient::packTo(int left, int top)
{
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;

    const int oldScreen = screen();
    move(left, top);
    if (screen() != oldScreen) {
        workspace()->sendClientToScreen(this, screen()); // checks rule validity
        if (maximizeMode() != MaximizeRestore)
            checkWorkspacePosition();
    }
}

/**
 * Moves active window left until in bumps into another window or workarea edge.
 */
void Workspace::slotWindowPackLeft()
{
    if (active_client && active_client->isMovable())
        active_client->packTo(packPositionLeft(active_client, active_client->frameGeometry().left(), true),
                              active_client->y());
}

void Workspace::slotWindowPackRight()
{
    if (active_client && active_client->isMovable())
        active_client->packTo(packPositionRight(active_client, active_client->frameGeometry().right(), true)
                                                - active_client->width() + 1, active_client->y());
}

void Workspace::slotWindowPackUp()
{
    if (active_client && active_client->isMovable())
        active_client->packTo(active_client->x(),
                              packPositionUp(active_client, active_client->frameGeometry().top(), true));
}

void Workspace::slotWindowPackDown()
{
    if (active_client && active_client->isMovable())
        active_client->packTo(active_client->x(),
                              packPositionDown(active_client, active_client->frameGeometry().bottom(), true) - active_client->height() + 1);
}

void Workspace::slotWindowGrowHorizontal()
{
    if (active_client)
        active_client->growHorizontal();
}

void AbstractClient::growHorizontal()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = frameGeometry();
    geom.setRight(workspace()->packPositionRight(this, geom.right(), true));
    QSize adjsize = constrainFrameSize(geom.size(), SizeModeFixedW);
    if (frameGeometry().size() == adjsize && geom.size() != adjsize && resizeIncrements().width() > 1) { // take care of size increments
        int newright = workspace()->packPositionRight(this, geom.right() + resizeIncrements().width() - 1, true);
        // check that it hasn't grown outside of the area, due to size increments
        // TODO this may be wrong?
        if (workspace()->clientArea(MovementArea,
                                   QPoint((x() + newright) / 2, frameGeometry().center().y()), desktop()).right() >= newright)
            geom.setRight(newright);
    }
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedW));
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedH));
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
    setFrameGeometry(geom);
}

void Workspace::slotWindowShrinkHorizontal()
{
    if (active_client)
        active_client->shrinkHorizontal();
}

void AbstractClient::shrinkHorizontal()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = frameGeometry();
    geom.setRight(workspace()->packPositionLeft(this, geom.right(), false));
    if (geom.width() <= 1)
        return;
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedW));
    if (geom.width() > 20) {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
        setFrameGeometry(geom);
    }
}

void Workspace::slotWindowGrowVertical()
{
    if (active_client)
        active_client->growVertical();
}

void AbstractClient::growVertical()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = frameGeometry();
    geom.setBottom(workspace()->packPositionDown(this, geom.bottom(), true));
    QSize adjsize = constrainFrameSize(geom.size(), SizeModeFixedH);
    if (frameGeometry().size() == adjsize && geom.size() != adjsize && resizeIncrements().height() > 1) { // take care of size increments
        int newbottom = workspace()->packPositionDown(this, geom.bottom() + resizeIncrements().height() - 1, true);
        // check that it hasn't grown outside of the area, due to size increments
        if (workspace()->clientArea(MovementArea,
                                   QPoint(frameGeometry().center().x(), (y() + newbottom) / 2), desktop()).bottom() >= newbottom)
            geom.setBottom(newbottom);
    }
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedH));
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
    setFrameGeometry(geom);
}


void Workspace::slotWindowShrinkVertical()
{
    if (active_client)
        active_client->shrinkVertical();
}

void AbstractClient::shrinkVertical()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = frameGeometry();
    geom.setBottom(workspace()->packPositionUp(this, geom.bottom(), false));
    if (geom.height() <= 1)
        return;
    geom.setSize(constrainFrameSize(geom.size(), SizeModeFixedH));
    if (geom.height() > 20) {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event;
        setFrameGeometry(geom);
    }
}

void Workspace::quickTileWindow(QuickTileMode mode)
{
    if (!active_client) {
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
            ( (m_lastTilingMode == FLAG(Left) || m_lastTilingMode == FLAG(Right)) && (mode == FLAG(Top) || mode == FLAG(Bottom)) )
            ||
            ( (m_lastTilingMode == FLAG(Top) || m_lastTilingMode == FLAG(Bottom)) && (mode == FLAG(Left) || mode == FLAG(Right)) )
#undef FLAG
        ) {
            mode |= m_lastTilingMode;
        }
        m_quickTileCombineTimer->stop();
    }

    active_client->setQuickTileMode(mode, true);
}

int Workspace::packPositionLeft(const AbstractClient *client, int oldX, bool leftEdge) const
{
    int newX = clientArea(MaximizeArea, client).left();
    if (oldX <= newX) { // try another Xinerama screen
        newX = clientArea(MaximizeArea,
                          QPoint(client->frameGeometry().left() - 1, client->frameGeometry().center().y()), client->desktop()).left();
    }
    if (client->titlebarPosition() != AbstractClient::PositionLeft) {
        const int right = newX - client->frameMargins().left();
        QRect frameGeometry = client->frameGeometry();
        frameGeometry.moveRight(right);
        if (screens()->intersecting(frameGeometry) < 2) {
            newX = right;
        }
    }
    if (oldX <= newX) {
        return oldX;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const int x = leftEdge ? (*it)->frameGeometry().right() + 1 : (*it)->frameGeometry().left() - 1;
        if (x > newX && x < oldX
                && !(client->frameGeometry().top() > (*it)->frameGeometry().bottom()  // they overlap in Y direction
                     || client->frameGeometry().bottom() < (*it)->frameGeometry().top())) {
            newX = x;
        }
    }
    return newX;
}

int Workspace::packPositionRight(const AbstractClient *client, int oldX, bool rightEdge) const
{
    int newX = clientArea(MaximizeArea, client).right();
    if (oldX >= newX) { // try another Xinerama screen
        newX = clientArea(MaximizeArea,
                          QPoint(client->frameGeometry().right() + 1, client->frameGeometry().center().y()), client->desktop()).right();
    }
    if (client->titlebarPosition() != AbstractClient::PositionRight) {
        const int right = newX + client->frameMargins().right();
        QRect frameGeometry = client->frameGeometry();
        frameGeometry.moveRight(right);
        if (screens()->intersecting(frameGeometry) < 2) {
            newX = right;
        }
    }
    if (oldX >= newX) {
        return oldX;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const int x = rightEdge ? (*it)->frameGeometry().left() - 1 : (*it)->frameGeometry().right() + 1;
        if (x < newX && x > oldX
                && !(client->frameGeometry().top() > (*it)->frameGeometry().bottom()
                     || client->frameGeometry().bottom() < (*it)->frameGeometry().top())) {
            newX = x;
        }
    }
    return newX;
}

int Workspace::packPositionUp(const AbstractClient *client, int oldY, bool topEdge) const
{
    int newY = clientArea(MaximizeArea, client).top();
    if (oldY <= newY) { // try another Xinerama screen
        newY = clientArea(MaximizeArea,
                          QPoint(client->frameGeometry().center().x(), client->frameGeometry().top() - 1), client->desktop()).top();
    }
    if (client->titlebarPosition() != AbstractClient::PositionTop) {
        const int top = newY - client->frameMargins().top();
        QRect frameGeometry = client->frameGeometry();
        frameGeometry.moveTop(top);
        if (screens()->intersecting(frameGeometry) < 2) {
            newY = top;
        }
    }
    if (oldY <= newY) {
        return oldY;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const int y = topEdge ? (*it)->frameGeometry().bottom() + 1 : (*it)->frameGeometry().top() - 1;
        if (y > newY && y < oldY
                && !(client->frameGeometry().left() > (*it)->frameGeometry().right()  // they overlap in X direction
                     || client->frameGeometry().right() < (*it)->frameGeometry().left())) {
            newY = y;
        }
    }
    return newY;
}

int Workspace::packPositionDown(const AbstractClient *client, int oldY, bool bottomEdge) const
{
    int newY = clientArea(MaximizeArea, client).bottom();
    if (oldY >= newY) { // try another Xinerama screen
        newY = clientArea(MaximizeArea,
                          QPoint(client->frameGeometry().center().x(), client->frameGeometry().bottom() + 1), client->desktop()).bottom();
    }
    if (client->titlebarPosition() != AbstractClient::PositionBottom) {
        const int bottom = newY + client->frameMargins().bottom();
        QRect frameGeometry = client->frameGeometry();
        frameGeometry.moveBottom(bottom);
        if (screens()->intersecting(frameGeometry) < 2) {
            newY = bottom;
        }
    }
    if (oldY >= newY) {
        return oldY;
    }
    const int desktop = client->desktop() == 0 || client->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : client->desktop();
    for (auto it = m_allClients.constBegin(), end = m_allClients.constEnd(); it != end; ++it) {
        if (isIrrelevant(*it, client, desktop)) {
            continue;
        }
        const int y = bottomEdge ? (*it)->frameGeometry().top() - 1 : (*it)->frameGeometry().bottom() + 1;
        if (y < newY && y > oldY
                && !(client->frameGeometry().left() > (*it)->frameGeometry().right()
                     || client->frameGeometry().right() < (*it)->frameGeometry().left())) {
            newY = y;
        }
    }
    return newY;
}

#endif

} // namespace
