/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 1997 to 2002 Cristian Tibirna <tibirna@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "placement.h"

#include <QRect>
#include <assert.h>

#include <QTextStream>

#ifndef KCMRULES
#include "workspace.h"
#include "client.h"
#include "options.h"
#include "rules.h"
#endif

namespace KWin
{

#ifndef KCMRULES

Placement::Placement(Workspace* w)
{
    m_WorkspacePtr = w;

    reinitCascading(0);
}

/*!
  Places the client \a c according to the workspace's layout policy
 */
void Placement::place(Client* c, QRect& area)
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
    else
        place(c, area, options->placement());
}

void Placement::place(Client* c, QRect& area, Policy policy, Policy nextPlacement)
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
}

/*!
  Place the client \a c according to a simply "random" placement algorithm.
 */
void Placement::placeAtRandom(Client* c, const QRect& area, Policy /*next*/)
{
    const int step  = 24;
    static int px = step;
    static int py = 2 * step;
    int tx, ty;

    const QRect maxRect = checkArea(c, area);

    if (px < maxRect.x())
        px = maxRect.x();
    if (py < maxRect.y())
        py = maxRect.y();

    px += step;
    py += 2 * step;

    if (px > maxRect.width() / 2)
        px =  maxRect.x() + step;
    if (py > maxRect.height() / 2)
        py =  maxRect.y() + step;
    tx = px;
    ty = py;
    if (tx + c->width() > maxRect.right()) {
        tx = maxRect.right() - c->width();
        if (tx < 0)
            tx = 0;
        px =  maxRect.x();
    }
    if (ty + c->height() > maxRect.bottom()) {
        ty = maxRect.bottom() - c->height();
        if (ty < 0)
            ty = 0;
        py =  maxRect.y();
    }
    c->move(tx, ty);
}

/*!
  Place the client \a c according to a really smart placement algorithm :-)
*/
void Placement::placeSmart(Client* c, const QRect& area, Policy /*next*/)
{
    /*
     * SmartPlacement by Cristian Tibirna (tibirna@kde.org)
     * adapted for kwm (16-19jan98) and for kwin (16Nov1999) using (with
     * permission) ideas from fvwm, authored by
     * Anthony Martin (amartin@engr.csulb.edu).
     * Xinerama supported added by Balaji Ramani (balaji@yablibli.com)
     * with ideas from xfce.
     */

    const int none = 0, h_wrong = -1, w_wrong = -2; // overlap types
    long int overlap, min_overlap = 0;
    int x_optimal, y_optimal;
    int possible;
    int desktop = c->desktop() == 0 || c->isOnAllDesktops() ? m_WorkspacePtr->currentDesktop() : c->desktop();

    int cxl, cxr, cyt, cyb;     //temp coords
    int  xl, xr, yt, yb;     //temp coords
    int basket;                 //temp holder

    // get the maximum allowed windows space
    const QRect maxRect = checkArea(c, area);
    int x = maxRect.left(), y = maxRect.top();
    x_optimal = x; y_optimal = y;

    //client gabarit
    int ch = c->height() - 1;
    int cw = c->width()  - 1;

    bool first_pass = true; //CT lame flag. Don't like it. What else would do?

    //loop over possible positions
    do {
        //test if enough room in x and y directions
        if (y + ch > maxRect.bottom() && ch < maxRect.height())
            overlap = h_wrong; // this throws the algorithm to an exit
        else if (x + cw > maxRect.right())
            overlap = w_wrong;
        else {
            overlap = none; //initialize

            cxl = x; cxr = x + cw;
            cyt = y; cyb = y + ch;
            ClientList::ConstIterator l;
            for (l = m_WorkspacePtr->stackingOrder().constBegin(); l != m_WorkspacePtr->stackingOrder().constEnd() ; ++l) {
                if ((*l)->isOnDesktop(desktop) &&
                        (*l)->isShown(false) && (*l) != c) {

                    xl = (*l)->x();          yt = (*l)->y();
                    xr = xl + (*l)->width(); yb = yt + (*l)->height();

                    //if windows overlap, calc the overall overlapping
                    if ((cxl < xr) && (cxr > xl) &&
                            (cyt < yb) && (cyb > yt)) {
                        xl = qMax(cxl, xl); xr = qMin(cxr, xr);
                        yt = qMax(cyt, yt); yb = qMin(cyb, yb);
                        if ((*l)->keepAbove())
                            overlap += 16 * (xr - xl) * (yb - yt);
                        else if ((*l)->keepBelow() && !(*l)->isDock()) // ignore KeepBelow windows
                            overlap += 0; // for placement (see Client::belongsToLayer() for Dock)
                        else
                            overlap += (xr - xl) * (yb - yt);
                    }
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

            possible = maxRect.right();
            if (possible - cw > x) possible -= cw;

            // compare to the position of each client on the same desk
            ClientList::ConstIterator l;
            for (l = m_WorkspacePtr->stackingOrder().constBegin(); l != m_WorkspacePtr->stackingOrder().constEnd() ; ++l) {

                if ((*l)->isOnDesktop(desktop) &&
                        (*l)->isShown(false) && (*l) != c) {

                    xl = (*l)->x();          yt = (*l)->y();
                    xr = xl + (*l)->width(); yb = yt + (*l)->height();

                    // if not enough room above or under the current tested client
                    // determine the first non-overlapped x position
                    if ((y < yb) && (yt < ch + y)) {

                        if ((xr > x) && (possible > xr)) possible = xr;

                        basket = xl - cw;
                        if ((basket > x) && (possible > basket)) possible = basket;
                    }
                }
            }
            x = possible;
        }

        // ... else ==> not enough x dimension (overlap was wrong on horizontal)
        else if (overlap == w_wrong) {
            x = maxRect.left();
            possible = maxRect.bottom();

            if (possible - ch > y) possible -= ch;

            //test the position of each window on the desk
            ClientList::ConstIterator l;
            for (l = m_WorkspacePtr->stackingOrder().constBegin(); l != m_WorkspacePtr->stackingOrder().constEnd() ; ++l) {
                if ((*l)->isOnDesktop(desktop) &&
                        (*l) != c   &&  c->isShown(false)) {

                    xl = (*l)->x();          yt = (*l)->y();
                    xr = xl + (*l)->width(); yb = yt + (*l)->height();

                    // if not enough room to the left or right of the current tested client
                    // determine the first non-overlapped y position
                    if ((yb > y) && (possible > yb)) possible = yb;

                    basket = yt - ch;
                    if ((basket > y) && (possible > basket)) possible = basket;
                }
            }
            y = possible;
        }
    } while ((overlap != none) && (overlap != h_wrong) && (y < maxRect.bottom()));

    if (ch >= maxRect.height())
        y_optimal = maxRect.top();

    // place the window
    c->move(x_optimal, y_optimal);

}

void Placement::reinitCascading(int desktop)
{
    // desktop == 0 - reinit all
    if (desktop == 0) {
        cci.clear();
        for (int i = 0; i < m_WorkspacePtr->numberOfDesktops(); i++) {
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

/*!
  Place windows in a cascading order, remembering positions for each desktop
*/
void Placement::placeCascaded(Client* c, QRect& area, Policy nextPlacement)
{
    /* cascadePlacement by Cristian Tibirna (tibirna@kde.org) (30Jan98)
     */
    // work coords
    int xp, yp;

    //CT how do I get from the 'Client' class the size that NW squarish "handle"
    const int delta_x = 24;
    const int delta_y = 24;

    const int dn = c->desktop() == 0 || c->isOnAllDesktops() ? (m_WorkspacePtr->currentDesktop() - 1) : (c->desktop() - 1);

    // get the maximum allowed windows space and desk's origin
    QRect maxRect = checkArea(c, area);

    // initialize often used vars: width and height of c; we gain speed
    const int ch = c->height();
    const int cw = c->width();
    const int X = maxRect.left();
    const int Y = maxRect.top();
    const int H = maxRect.height();
    const int W = maxRect.width();

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
         * if (xp != X && yp == Y) xp = delta_x * (++(cci[dn].col));
         * if (yp != Y && xp == X) yp = delta_y * (++(cci[dn].row));
         */
        if (xp != X && yp == Y) {
            ++(cci[dn].col);
            xp = delta_x * cci[dn].col;
        }
        if (yp != Y && xp == X) {
            ++(cci[dn].row);
            yp = delta_y * cci[dn].row;
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
    cci[dn].pos = QPoint(xp + delta_x, yp + delta_y);
}

/*!
  Place windows centered, on top of all others
*/
void Placement::placeCentered(Client* c, const QRect& area, Policy /*next*/)
{

    // get the maximum allowed windows space and desk's origin
    const QRect maxRect = checkArea(c, area);

    const int xp = maxRect.left() + (maxRect.width() -  c->width())  / 2;
    const int yp = maxRect.top()  + (maxRect.height() - c->height()) / 2;

    // place the window
    c->move(QPoint(xp, yp));
}

/*!
  Place windows in the (0,0) corner, on top of all others
*/
void Placement::placeZeroCornered(Client* c, const QRect& area, Policy /*next*/)
{
    // get the maximum allowed windows space and desk's origin
    const QRect maxRect = checkArea(c, area);

    // place the window
    c->move(QPoint(maxRect.left(), maxRect.top()));
}

void Placement::placeUtility(Client* c, QRect& area, Policy /*next*/)
{
// TODO kwin should try to place utility windows next to their mainwindow,
// preferably at the right edge, and going down if there are more of them
// if there's not enough place outside the mainwindow, it should prefer
// top-right corner
    // use the default placement for now
    place(c, area, Default);
}


void Placement::placeDialog(Client* c, QRect& area, Policy nextPlacement)
{
    placeOnMainWindow(c, area, nextPlacement);
}

void Placement::placeUnderMouse(Client* c, QRect& area, Policy /*next*/)
{
    area = checkArea(c, area);
    QRect geom = c->geometry();
    geom.moveCenter(cursorPos());
    c->move(geom.topLeft());
    c->keepInArea(area);   // make sure it's kept inside workarea
}

void Placement::placeOnMainWindow(Client* c, QRect& area, Policy nextPlacement)
{
    if (nextPlacement == Unknown)
        nextPlacement = Centered;
    if (nextPlacement == Maximizing)   // maximize if needed
        placeMaximizing(c, area, NoPlacement);
    area = checkArea(c, area);
    ClientList mainwindows = c->mainClients();
    Client* place_on = NULL;
    Client* place_on2 = NULL;
    int mains_count = 0;
    for (ClientList::ConstIterator it = mainwindows.constBegin();
            it != mainwindows.constEnd();
            ++it) {
        if (mainwindows.count() > 1 && (*it)->isSpecialWindow())
            continue; // don't consider toolbars etc when placing
        ++mains_count;
        place_on2 = *it;
        if ((*it)->isOnCurrentDesktop()) {
            if (place_on == NULL)
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
    if (place_on == NULL) {
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
    QRect geom = c->geometry();
    geom.moveCenter(place_on->geometry().center());
    c->move(geom.topLeft());
    // get area again, because the mainwindow may be on different xinerama screen
    area = checkArea(c, QRect());
    c->keepInArea(area);   // make sure it's kept inside workarea
}

void Placement::placeMaximizing(Client* c, QRect& area, Policy nextPlacement)
{
    if (nextPlacement == Unknown)
        nextPlacement = Smart;
    if (c->isMaximizable() && c->maxSize().width() >= area.width() && c->maxSize().height() >= area.height()) {
        if (m_WorkspacePtr->clientArea(MaximizeArea, c) == area)
            c->maximize(Client::MaximizeFull);
        else { // if the geometry doesn't match default maximize area (xinerama case?),
            // it's probably better to use the given area
            c->setGeometry(area);
        }
    } else {
        c->resizeWithChecks(c->maxSize().boundedTo(area.size()));
        place(c, area, nextPlacement);
    }
}

QRect Placement::checkArea(const Client* c, const QRect& area)
{
    if (area.isNull())
        return m_WorkspacePtr->clientArea(PlacementArea, c->geometry().center(), c->desktop());
    return area;
}

#endif


Placement::Policy Placement::policyFromString(const QString& policy, bool no_special)
{
    if (policy == "NoPlacement")
        return NoPlacement;
    else if (policy == "Default" && !no_special)
        return Default;
    else if (policy == "Random")
        return Random;
    else if (policy == "Cascade")
        return Cascade;
    else if (policy == "Centered")
        return Centered;
    else if (policy == "ZeroCornered")
        return ZeroCornered;
    else if (policy == "UnderMouse" && !no_special)
        return UnderMouse;
    else if (policy == "OnMainWindow" && !no_special)
        return OnMainWindow;
    else if (policy == "Maximizing")
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
    assert(policy < int(sizeof(policies) / sizeof(policies[ 0 ])));
    return policies[ policy ];
}


#ifndef KCMRULES

// ********************
// Workspace
// ********************

/*!
  Moves active window left until in bumps into another window or workarea edge.
 */
void Workspace::slotWindowPackLeft()
{
    if (active_client && active_client->isMovable())
        active_client->move(packPositionLeft(active_client, active_client->geometry().left(), true),
                            active_client->y());
}

void Workspace::slotWindowPackRight()
{
    if (active_client && active_client->isMovable())
        active_client->move(
            packPositionRight(active_client, active_client->geometry().right(), true)
            - active_client->width() + 1, active_client->y());
}

void Workspace::slotWindowPackUp()
{
    if (active_client && active_client->isMovable())
        active_client->move(active_client->x(),
                            packPositionUp(active_client, active_client->geometry().top(), true));
}

void Workspace::slotWindowPackDown()
{
    if (active_client && active_client->isMovable())
        active_client->move(active_client->x(),
                            packPositionDown(active_client, active_client->geometry().bottom(), true) - active_client->height() + 1);
}

void Workspace::slotWindowGrowHorizontal()
{
    if (active_client)
        active_client->growHorizontal();
}

void Client::growHorizontal()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = geometry();
    geom.setRight(workspace()->packPositionRight(this, geom.right(), true));
    QSize adjsize = adjustedSize(geom.size(), SizemodeFixedW);
    if (geometry().size() == adjsize && geom.size() != adjsize && xSizeHint.width_inc > 1) { // take care of size increments
        int newright = workspace()->packPositionRight(this, geom.right() + xSizeHint.width_inc - 1, true);
        // check that it hasn't grown outside of the area, due to size increments
        // TODO this may be wrong?
        if (workspace()->clientArea(MovementArea,
                                   QPoint((x() + newright) / 2, geometry().center().y()), desktop()).right() >= newright)
            geom.setRight(newright);
    }
    geom.setSize(adjustedSize(geom.size(), SizemodeFixedW));
    setGeometry(geom);
}

void Workspace::slotWindowShrinkHorizontal()
{
    if (active_client)
        active_client->shrinkHorizontal();
}

void Client::shrinkHorizontal()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = geometry();
    geom.setRight(workspace()->packPositionLeft(this, geom.right(), false));
    if (geom.width() <= 1)
        return;
    geom.setSize(adjustedSize(geom.size(), SizemodeFixedW));
    if (geom.width() > 20)
        setGeometry(geom);
}

void Workspace::slotWindowGrowVertical()
{
    if (active_client)
        active_client->growVertical();
}

void Client::growVertical()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = geometry();
    geom.setBottom(workspace()->packPositionDown(this, geom.bottom(), true));
    QSize adjsize = adjustedSize(geom.size(), SizemodeFixedH);
    if (geometry().size() == adjsize && geom.size() != adjsize && xSizeHint.height_inc > 1) { // take care of size increments
        int newbottom = workspace()->packPositionDown(this, geom.bottom() + xSizeHint.height_inc - 1, true);
        // check that it hasn't grown outside of the area, due to size increments
        if (workspace()->clientArea(MovementArea,
                                   QPoint(geometry().center().x(), (y() + newbottom) / 2), desktop()).bottom() >= newbottom)
            geom.setBottom(newbottom);
    }
    geom.setSize(adjustedSize(geom.size(), SizemodeFixedH));
    setGeometry(geom);
}


void Workspace::slotWindowShrinkVertical()
{
    if (active_client)
        active_client->shrinkVertical();
}

void Client::shrinkVertical()
{
    if (!isResizable() || isShade())
        return;
    QRect geom = geometry();
    geom.setBottom(workspace()->packPositionUp(this, geom.bottom(), false));
    if (geom.height() <= 1)
        return;
    geom.setSize(adjustedSize(geom.size(), SizemodeFixedH));
    if (geom.height() > 20)
        setGeometry(geom);
}


void Workspace::slotWindowQuickTileLeft()
{
    if (!active_client)
        return;

    active_client->setQuickTileMode(QuickTileLeft, true);
}

void Workspace::slotWindowQuickTileRight()
{
    if (!active_client)
        return;

    active_client->setQuickTileMode(QuickTileRight, true);
}

void Workspace::slotWindowQuickTileTopLeft()
{
    if (!active_client) {
        return;
    }
    active_client->setQuickTileMode(QuickTileTop|QuickTileLeft, true);
}

void Workspace::slotWindowQuickTileTopRight()
{
    if (!active_client) {
        return;
    }
    active_client->setQuickTileMode(QuickTileTop|QuickTileRight, true);
}

void Workspace::slotWindowQuickTileBottomLeft()
{
    if (!active_client) {
        return;
    }
    active_client->setQuickTileMode(QuickTileBottom|QuickTileLeft, true);
}

void Workspace::slotWindowQuickTileBottomRight()
{
    if (!active_client) {
        return;
    }
    active_client->setQuickTileMode(QuickTileBottom|QuickTileRight, true);
}

int Workspace::packPositionLeft(const Client* cl, int oldx, bool left_edge) const
{
    int newx = clientArea(MovementArea, cl).left();
    if (oldx <= newx)   // try another Xinerama screen
        newx = clientArea(MovementArea,
                          QPoint(cl->geometry().left() - 1, cl->geometry().center().y()), cl->desktop()).left();
    if (oldx <= newx)
        return oldx;
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if (!(*it)->isShown(false) || !(*it)->isOnDesktop(active_client->desktop()))
            continue;
        int x = left_edge ? (*it)->geometry().right() + 1 : (*it)->geometry().left() - 1;
        if (x > newx && x < oldx
                && !(cl->geometry().top() > (*it)->geometry().bottom()  // they overlap in Y direction
                     || cl->geometry().bottom() < (*it)->geometry().top()))
            newx = x;
    }
    return newx;
}

int Workspace::packPositionRight(const Client* cl, int oldx, bool right_edge) const
{
    int newx = clientArea(MovementArea, cl).right();
    if (oldx >= newx)   // try another Xinerama screen
        newx = clientArea(MovementArea,
                          QPoint(cl->geometry().right() + 1, cl->geometry().center().y()), cl->desktop()).right();
    if (oldx >= newx)
        return oldx;
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if (!(*it)->isShown(false) || !(*it)->isOnDesktop(cl->desktop()))
            continue;
        int x = right_edge ? (*it)->geometry().left() - 1 : (*it)->geometry().right() + 1;
        if (x < newx && x > oldx
                && !(cl->geometry().top() > (*it)->geometry().bottom()
                     || cl->geometry().bottom() < (*it)->geometry().top()))
            newx = x;
    }
    return newx;
}

int Workspace::packPositionUp(const Client* cl, int oldy, bool top_edge) const
{
    int newy = clientArea(MovementArea, cl).top();
    if (oldy <= newy)   // try another Xinerama screen
        newy = clientArea(MovementArea,
                          QPoint(cl->geometry().center().x(), cl->geometry().top() - 1), cl->desktop()).top();
    if (oldy <= newy)
        return oldy;
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if (!(*it)->isShown(false) || !(*it)->isOnDesktop(cl->desktop()))
            continue;
        int y = top_edge ? (*it)->geometry().bottom() + 1 : (*it)->geometry().top() - 1;
        if (y > newy && y < oldy
                && !(cl->geometry().left() > (*it)->geometry().right()  // they overlap in X direction
                     || cl->geometry().right() < (*it)->geometry().left()))
            newy = y;
    }
    return newy;
}

int Workspace::packPositionDown(const Client* cl, int oldy, bool bottom_edge) const
{
    int newy = clientArea(MovementArea, cl).bottom();
    if (oldy >= newy)   // try another Xinerama screen
        newy = clientArea(MovementArea,
                          QPoint(cl->geometry().center().x(), cl->geometry().bottom() + 1), cl->desktop()).bottom();
    if (oldy >= newy)
        return oldy;
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if (!(*it)->isShown(false) || !(*it)->isOnDesktop(cl->desktop()))
            continue;
        int y = bottom_edge ? (*it)->geometry().top() - 1 : (*it)->geometry().bottom() + 1;
        if (y < newy && y > oldy
                && !(cl->geometry().left() > (*it)->geometry().right()
                     || cl->geometry().right() < (*it)->geometry().left()))
            newy = y;
    }
    return newy;
}

/*!
  Asks the internal positioning object to place a client
*/
void Workspace::place(Client* c, QRect& area)
{
    initPositioning->place(c, area);
}

void Workspace::placeSmart(Client* c, const QRect& area)
{
    initPositioning->placeSmart(c, area);
}

#endif

} // namespace
