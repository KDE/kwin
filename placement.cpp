/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>


Placement algorithms
Copyright (C) 1997 to 2002 Cristian Tibirna <tibirna@kde.org>
******************************************************************/

#include "workspace.h"
#include "client.h"
#include "options.h"
#include "placement.h"

#include <qrect.h>


using namespace KWinInternal;

namespace KWinInternal {

class PlacementPrivate
{
public:
    //CT needed for cascading+
    struct DesktopCascadingInfo {
      QPoint pos;
      int col;
      int row;
    };

    QValueList<DesktopCascadingInfo> cci;

    Workspace* m_WorkspacePtr;
};

};

Placement::Placement(Workspace* w)
{
    d = new PlacementPrivate;
    
    d->m_WorkspacePtr = w;

    //CT initialize the cascading info
    for( int i = 0; i < d->m_WorkspacePtr->numberOfDesktops(); i++) {
      PlacementPrivate::DesktopCascadingInfo inf;
      inf.pos = QPoint(0,0);
      inf.col = 0;
      inf.row = 0;
      d->cci.append(inf);
    }

}

Placement::~Placement()
{
    delete d;
}

/*!
  Places the client \a c according to the workspace's layout policy
 */
void Placement::place(Client* c)
{
    if      (options->placement == Options::Random)               placeAtRandom(c);
    else if (options->placement == Options::Cascade)              placeCascaded(c);
    else if (options->placement == Options::Centered)     placeCentered(c);
    else if (options->placement == Options::ZeroCornered) placeZeroCornered(c);
    else                                                          placeSmart(c);
}

/*!
  Place the client \a c according to a simply "random" placement algorithm.
 */
void Placement::placeAtRandom(Client* c)
{
    const int step  = 24;
    static int px = step;
    static int py = 2 * step;
    int tx,ty;

    const QRect maxRect = d->m_WorkspacePtr->clientArea(Workspace::PlacementArea);

    if (px < maxRect.x())
        px = maxRect.x();
    if (py < maxRect.y())
        py = maxRect.y();

    px += step;
    py += 2*step;

    if (px > maxRect.width()/2)
        px =  maxRect.x() + step;
    if (py > maxRect.height()/2)
        py =  maxRect.y() + step;
    tx = px;
    ty = py;
    if (tx + c->width() > maxRect.right()){
        tx = maxRect.right() - c->width();
        if (tx < 0)
            tx = 0;
        px =  maxRect.x();
    }
    if (ty + c->height() > maxRect.bottom()){
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
void Placement::placeSmart(Client* c)
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
    int desktop = c->desktop() < 0 || c->isSticky() ? d->m_WorkspacePtr->currentDesktop() : c->desktop();

    int cxl, cxr, cyt, cyb;     //temp coords
    int  xl,  xr,  yt,  yb;     //temp coords
    int basket;                 //temp holder

    // get the maximum allowed windows space
    const QRect maxRect = d->m_WorkspacePtr->clientArea(Workspace::PlacementArea);
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
        else if(x + cw > maxRect.right())
            overlap = w_wrong;
        else {
            overlap = none; //initialize

            cxl = x; cxr = x + cw;
            cyt = y; cyb = y + ch;
            ClientList::ConstIterator l;
            for(l = d->m_WorkspacePtr->stackingOrder().begin(); l != d->m_WorkspacePtr->stackingOrder().end() ; ++l) {
                if((*l)->isOnDesktop(desktop) &&
                   !(*l)->isIconified() && (*l) != c) {

                    xl = (*l)->x();          yt = (*l)->y();
                    xr = xl + (*l)->width(); yb = yt + (*l)->height();

                    //if windows overlap, calc the overall overlapping
                    if((cxl < xr) && (cxr > xl) &&
                       (cyt < yb) && (cyb > yt)) {
                        xl = QMAX(cxl, xl); xr = QMIN(cxr, xr);
                        yt = QMAX(cyt, yt); yb = QMIN(cyb, yb);
                        if((*l)->staysOnTop())
                            overlap += 16 * (xr - xl) * (yb - yt);
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
            for(l = d->m_WorkspacePtr->stackingOrder().begin(); l != d->m_WorkspacePtr->stackingOrder().end() ; ++l) {

                if ((*l)->isOnDesktop(desktop) &&
                     !(*l)->isIconified() &&  (*l) != c) {

                    xl = (*l)->x();          yt = (*l)->y();
                    xr = xl + (*l)->width(); yb = yt + (*l)->height();

                    // if not enough room above or under the current tested client
                    // determine the first non-overlapped x position
                    if((y < yb) && (yt < ch + y)) {

                        if((xr > x) && (possible > xr)) possible = xr;

                        basket = xl - cw;
                        if((basket > x) && (possible > basket)) possible = basket;
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
            for(l = d->m_WorkspacePtr->stackingOrder().begin(); l != d->m_WorkspacePtr->stackingOrder().end() ; ++l) {
                if((*l)->isOnDesktop(desktop) &&
                    (*l) != c   &&  !c->isIconified()) {

                    xl = (*l)->x();          yt = (*l)->y();
                    xr = xl + (*l)->width(); yb = yt + (*l)->height();

                    // if not enough room to the left or right of the current tested client
                    // determine the first non-overlapped y position
                    if((yb > y) && (possible > yb)) possible = yb;

                    basket = yt - ch;
                    if((basket > y) && (possible > basket)) possible = basket;
                }
            }
            y = possible;
        }
    }
    while((overlap != none) && (overlap != h_wrong) && (y < maxRect.bottom()));

    if(ch>= maxRect.height())
        y_optimal=maxRect.top();

    // place the window
    c->move(x_optimal, y_optimal);

}

/*!
  Place windows in a cascading order, remembering positions for each desktop
*/
void Placement::placeCascaded (Client* c, bool re_init)
{
    /* cascadePlacement by Cristian Tibirna (tibirna@kde.org) (30Jan98)
     */
    // work coords
    int xp, yp;

    //CT how do I get from the 'Client' class the size that NW squarish "handle"
    const int delta_x = 24;
    const int delta_y = 24;

    const int dn = c->desktop() < 0 || c->isSticky() ? (d->m_WorkspacePtr->currentDesktop() - 1) : (c->desktop() - 1);

    // get the maximum allowed windows space and desk's origin
    //    (CT 20Nov1999 - is this common to all desktops?)
    QRect maxRect = d->m_WorkspacePtr->clientArea(Workspace::PlacementArea);

    // initialize often used vars: width and height of c; we gain speed
    const int ch = c->height();
    const int cw = c->width();
    const int X = maxRect.left();
    const int Y = maxRect.top();
    const int H = maxRect.height();
    const int W = maxRect.width();

  //initialize if needed
    if (re_init) {
        d->cci[dn].pos = QPoint(X, Y);
        d->cci[dn].col = d->cci[dn].row = 0;
    }


    xp = d->cci[dn].pos.x();
    yp = d->cci[dn].pos.y();

    //here to touch in case people vote for resize on placement
    if ((yp + ch) > H) yp = Y;

    if ((xp + cw) > W)
        if (!yp) {
            placeSmart(c);
            return;
        }
        else xp = X;

  //if this isn't the first window
    if (d->cci[dn].pos.x() != X && d->cci[dn].pos.y() != Y) {
                /* The following statements cause an internal compiler error with
                 * egcs-2.91.66 on SuSE Linux 6.3. The equivalent forms compile fine.
                 * 22-Dec-1999 CS
                 *
                 * if (xp != X && yp == Y) xp = delta_x * (++(d->cci[dn].col));
                 * if (yp != Y && xp == X) yp = delta_y * (++(d->cci[dn].row));
                 */
        if (xp != X && yp == Y)
        {
                ++(d->cci[dn].col);
                xp = delta_x * d->cci[dn].col;
        }
        if (yp != Y && xp == X)
        {
                ++(d->cci[dn].row);
                yp = delta_y * d->cci[dn].row;
        }

        // last resort: if still doesn't fit, smart place it
        if (((xp + cw) > W - X) || ((yp + ch) > H - Y)) {
            placeSmart(c);
            return;
        }
    }

    // place the window
    c->move(QPoint(xp, yp));

    // new position
    d->cci[dn].pos = QPoint(xp + delta_x,  yp + delta_y);
}

/*!
  Place windows centered, on top of all others
*/
void Placement::placeCentered (Client* c){

    // get the maximum allowed windows space and desk's origin
    //    (CT 20Nov1999 - is this common to all desktops?)
    const QRect maxRect = d->m_WorkspacePtr->clientArea(Workspace::PlacementArea);

    const int xp = maxRect.left() + (maxRect.width() -  c->width())  / 2;
    const int yp = maxRect.top()  + (maxRect.height() - c->height()) / 2;

    // place the window
    c->move(QPoint(xp, yp));
}

/*!
  Place windows in the (0,0) corner, on top of all others
*/
void Placement::placeZeroCornered(Client* c)
{
    // get the maximum allowed windows space and desk's origin
    //    (CT 20Nov1999 - is this common to all desktops?)
    const QRect maxRect = d->m_WorkspacePtr->clientArea(Workspace::PlacementArea);

    // place the window
    c->move(QPoint(maxRect.left(), maxRect.top()));
}
