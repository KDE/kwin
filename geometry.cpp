/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

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

/*

 This file contains things relevant to geometry, i.e. workspace size,
 window positions and window sizes.

*/

#include "client.h"
#include "composite.h"
#include "cursor.h"
#include "netinfo.h"
#include "workspace.h"

#include "placement.h"
#include "geometrytip.h"
#include "rules.h"
#include "screens.h"
#include "effects.h"
#include "screenedge.h"
#include <QApplication>
#include <QDebug>
#include <QVarLengthArray>

#include "outline.h"
#include "shell_client.h"
#include "wayland_server.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

namespace KWin
{

static inline int sign(int v) {
    return (v > 0) - (v < 0);
}

//********************************************
// Workspace
//********************************************

extern int screen_number;
extern bool is_multihead;

/*!
  Resizes the workspace after an XRANDR screen size change
 */
void Workspace::desktopResized()
{
    QRect geom = screens()->geometry();
    if (rootInfo()) {
        NETSize desktop_geometry;
        desktop_geometry.width = geom.width();
        desktop_geometry.height = geom.height();
        rootInfo()->setDesktopGeometry(desktop_geometry);
    }

    updateClientArea();
    saveOldScreenSizes(); // after updateClientArea(), so that one still uses the previous one

    // TODO: emit a signal instead and remove the deep function calls into edges and effects
    ScreenEdges::self()->recreateEdges();

    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->desktopResized(geom.size());
    }
}

void Workspace::saveOldScreenSizes()
{
    olddisplaysize = screens()->displaySize();
    oldscreensizes.clear();
    for( int i = 0;
         i < screens()->count();
         ++i )
        oldscreensizes.append( screens()->geometry( i ));
}

/*!
  Updates the current client areas according to the current clients.

  If the area changes or force is true, the new areas are propagated to the world.

  The client area is the area that is available for clients (that
  which is not taken by windows like panels, the top-of-screen menu
  etc).

  \sa clientArea()
 */

void Workspace::updateClientArea(bool force)
{
    const Screens *s = Screens::self();
    int nscreens = s->count();
    const int numberOfDesktops = VirtualDesktopManager::self()->count();
    QVector< QRect > new_wareas(numberOfDesktops + 1);
    QVector< StrutRects > new_rmoveareas(numberOfDesktops + 1);
    QVector< QVector< QRect > > new_sareas(numberOfDesktops + 1);
    QVector< QRect > screens(nscreens);
    QRect desktopArea;
    for (int i = 0; i < nscreens; i++) {
        desktopArea |= s->geometry(i);
    }
    for (int iS = 0;
            iS < nscreens;
            iS ++) {
        screens [iS] = s->geometry(iS);
    }
    for (int i = 1;
            i <= numberOfDesktops;
            ++i) {
        new_wareas[ i ] = desktopArea;
        new_sareas[ i ].resize(nscreens);
        for (int iS = 0;
                iS < nscreens;
                iS ++)
            new_sareas[ i ][ iS ] = screens[ iS ];
    }
    for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it) {
        if (!(*it)->hasStrut())
            continue;
        QRect r = (*it)->adjustedClientArea(desktopArea, desktopArea);
        // sanity check that a strut doesn't exclude a complete screen geometry
        // this is a violation to EWMH, as KWin just ignores the strut
        for (int i = 0; i < Screens::self()->count(); i++) {
            if (!r.intersects(Screens::self()->geometry(i))) {
                qCDebug(KWIN_CORE) << "Adjusted client area would exclude a complete screen, ignore";
                r = desktopArea;
                break;
            }
        }
        StrutRects strutRegion = (*it)->strutRects();
        const QRect clientsScreenRect = KWin::screens()->geometry((*it)->screen());
        for (auto strut = strutRegion.begin(); strut != strutRegion.end(); strut++) {
            *strut = StrutRect((*strut).intersected(clientsScreenRect), (*strut).area());
        }

        // Ignore offscreen xinerama struts. These interfere with the larger monitors on the setup
        // and should be ignored so that applications that use the work area to work out where
        // windows can go can use the entire visible area of the larger monitors.
        // This goes against the EWMH description of the work area but it is a toss up between
        // having unusable sections of the screen (Which can be quite large with newer monitors)
        // or having some content appear offscreen (Relatively rare compared to other).
        bool hasOffscreenXineramaStrut = (*it)->hasOffscreenXineramaStrut();

        if ((*it)->isOnAllDesktops()) {
            for (int i = 1;
                    i <= numberOfDesktops;
                    ++i) {
                if (!hasOffscreenXineramaStrut)
                    new_wareas[ i ] = new_wareas[ i ].intersected(r);
                new_rmoveareas[ i ] += strutRegion;
                for (int iS = 0;
                        iS < nscreens;
                        iS ++) {
                    const auto geo = new_sareas[ i ][ iS ].intersected(
                                                (*it)->adjustedClientArea(desktopArea, screens[ iS ]));
                    // ignore the geometry if it results in the screen getting removed completly
                    if (!geo.isEmpty()) {
                        new_sareas[ i ][ iS ] = geo;
                    }
                }
            }
        } else {
            if (!hasOffscreenXineramaStrut)
                new_wareas[(*it)->desktop()] = new_wareas[(*it)->desktop()].intersected(r);
            new_rmoveareas[(*it)->desktop()] += strutRegion;
            for (int iS = 0;
                    iS < nscreens;
                    iS ++) {
//                            qDebug() << "adjusting new_sarea: " << screens[ iS ];
                const auto geo = new_sareas[(*it)->desktop()][ iS ].intersected(
                      (*it)->adjustedClientArea(desktopArea, screens[ iS ]));
                // ignore the geometry if it results in the screen getting removed completly
                if (!geo.isEmpty()) {
                    new_sareas[(*it)->desktop()][ iS ] = geo;
                }
            }
        }
    }
    if (waylandServer()) {
        auto updateStrutsForWaylandClient = [&] (ShellClient *c) {
            // assuming that only docks have "struts" and that all docks have a strut
            if (!c->hasStrut()) {
                return;
            }
            auto margins = [c] (const QRect &geometry) {
                QMargins margins;
                if (!geometry.intersects(c->geometry())) {
                    return margins;
                }
                // figure out which areas of the overall screen setup it borders
                const bool left = c->geometry().left() == geometry.left();
                const bool right = c->geometry().right() == geometry.right();
                const bool top = c->geometry().top() == geometry.top();
                const bool bottom = c->geometry().bottom() == geometry.bottom();
                const bool horizontal = c->geometry().width() >= c->geometry().height();
                if (left && ((!top && !bottom) || !horizontal)) {
                    margins.setLeft(c->geometry().width());
                }
                if (right && ((!top && !bottom) || !horizontal)) {
                    margins.setRight(c->geometry().width());
                }
                if (top && ((!left && !right) || horizontal)) {
                    margins.setTop(c->geometry().height());
                }
                if (bottom && ((!left && !right) || horizontal)) {
                    margins.setBottom(c->geometry().height());
                }
                return margins;
            };
            auto marginsToStrutArea = [] (const QMargins &margins) {
                if (margins.left() != 0) {
                    return StrutAreaLeft;
                }
                if (margins.right() != 0) {
                    return StrutAreaRight;
                }
                if (margins.top() != 0) {
                    return StrutAreaTop;
                }
                if (margins.bottom() != 0) {
                    return StrutAreaBottom;
                }
                return StrutAreaInvalid;
            };
            const auto strut = margins(KWin::screens()->geometry(c->screen()));
            const StrutRects strutRegion = StrutRects{StrutRect(c->geometry(), marginsToStrutArea(strut))};
            QRect r = desktopArea - margins(KWin::screens()->geometry());
            if (c->isOnAllDesktops()) {
                for (int i = 1; i <= numberOfDesktops; ++i) {
                    new_wareas[ i ] = new_wareas[ i ].intersected(r);
                    for (int iS = 0; iS < nscreens; ++iS) {
                        new_sareas[ i ][ iS ] = new_sareas[ i ][ iS ].intersected(screens[iS] - margins(screens[iS]));
                    }
                    new_rmoveareas[ i ] += strutRegion;
                }
            } else {
                new_wareas[c->desktop()] = new_wareas[c->desktop()].intersected(r);
                for (int iS = 0; iS < nscreens; iS++) {
                    new_sareas[c->desktop()][ iS ] = new_sareas[c->desktop()][ iS ].intersected(screens[iS] - margins(screens[iS]));
                }
                new_rmoveareas[ c->desktop() ] += strutRegion;
            }
        };
        const auto clients = waylandServer()->clients();
        for (auto c : clients) {
            updateStrutsForWaylandClient(c);
        }
        const auto internalClients = waylandServer()->internalClients();
        for (auto c : internalClients) {
            updateStrutsForWaylandClient(c);
        }
    }
#if 0
    for (int i = 1;
            i <= numberOfDesktops();
            ++i) {
        for (int iS = 0;
                iS < nscreens;
                iS ++)
            qCDebug(KWIN_CORE) << "new_sarea: " << new_sareas[ i ][ iS ];
    }
#endif

    bool changed = force;

    if (screenarea.isEmpty())
        changed = true;

    for (int i = 1;
            !changed && i <= numberOfDesktops;
            ++i) {
        if (workarea[ i ] != new_wareas[ i ])
            changed = true;
        if (restrictedmovearea[ i ] != new_rmoveareas[ i ])
            changed = true;
        if (screenarea[ i ].size() != new_sareas[ i ].size())
            changed = true;
        for (int iS = 0;
                !changed && iS < nscreens;
                iS ++)
            if (new_sareas[ i ][ iS ] != screenarea [ i ][ iS ])
                changed = true;
    }

    if (changed) {
        workarea = new_wareas;
        oldrestrictedmovearea = restrictedmovearea;
        restrictedmovearea = new_rmoveareas;
        screenarea = new_sareas;
        if (rootInfo()) {
            NETRect r;
            for (int i = 1; i <= numberOfDesktops; i++) {
                r.pos.x = workarea[ i ].x();
                r.pos.y = workarea[ i ].y();
                r.size.width = workarea[ i ].width();
                r.size.height = workarea[ i ].height();
                rootInfo()->setWorkArea(i, r);
            }
        }

        for (auto it = m_allClients.constBegin();
                it != m_allClients.constEnd();
                ++it)
            (*it)->checkWorkspacePosition();
        for (ClientList::ConstIterator it = desktops.constBegin();
                it != desktops.constEnd();
                ++it)
            (*it)->checkWorkspacePosition();

        oldrestrictedmovearea.clear(); // reset, no longer valid or needed
    }
}

void Workspace::updateClientArea()
{
    updateClientArea(false);
}


/*!
  returns the area available for clients. This is the desktop
  geometry minus windows on the dock.  Placement algorithms should
  refer to this rather than geometry().

  \sa geometry()
 */

QRect Workspace::clientArea(clientAreaOption opt, int screen, int desktop) const
{
    if (desktop == NETWinInfo::OnAllDesktops || desktop == 0)
        desktop = VirtualDesktopManager::self()->current();
    if (screen == -1)
        screen = screens()->current();
    const QSize displaySize = screens()->displaySize();

    QRect sarea, warea;

    if (is_multihead) {
        sarea = (!screenarea.isEmpty()
                   && screen < screenarea[ desktop ].size()) // screens may be missing during KWin initialization or screen config changes
                  ? screenarea[ desktop ][ screen_number ]
                  : screens()->geometry(screen_number);
        warea = workarea[ desktop ].isNull()
                ? screens()->geometry(screen_number)
                : workarea[ desktop ];
    } else {
        sarea = (!screenarea.isEmpty()
                && screen < screenarea[ desktop ].size()) // screens may be missing during KWin initialization or screen config changes
                ? screenarea[ desktop ][ screen ]
                : screens()->geometry(screen);
        warea = workarea[ desktop ].isNull()
                ? QRect(0, 0, displaySize.width(), displaySize.height())
                : workarea[ desktop ];
    }

    switch(opt) {
    case MaximizeArea:
    case PlacementArea:
            return sarea;
    case MaximizeFullArea:
    case FullScreenArea:
    case MovementArea:
    case ScreenArea:
        if (is_multihead)
            return screens()->geometry(screen_number);
        else
            return screens()->geometry(screen);
    case WorkArea:
        if (is_multihead)
            return sarea;
        else
            return warea;
    case FullArea:
        return QRect(0, 0, displaySize.width(), displaySize.height());
    }
    abort();
}


QRect Workspace::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return clientArea(opt, screens()->number(p), desktop);
}

QRect Workspace::clientArea(clientAreaOption opt, const AbstractClient* c) const
{
    return clientArea(opt, c->geometry().center(), c->desktop());
}

QRegion Workspace::restrictedMoveArea(int desktop, StrutAreas areas) const
{
    if (desktop == NETWinInfo::OnAllDesktops || desktop == 0)
        desktop = VirtualDesktopManager::self()->current();
    QRegion region;
    foreach (const StrutRect & rect, restrictedmovearea[desktop])
    if (areas & rect.area())
        region += rect;
    return region;
}

bool Workspace::inUpdateClientArea() const
{
    return !oldrestrictedmovearea.isEmpty();
}

QRegion Workspace::previousRestrictedMoveArea(int desktop, StrutAreas areas) const
{
    if (desktop == NETWinInfo::OnAllDesktops || desktop == 0)
        desktop = VirtualDesktopManager::self()->current();
    QRegion region;
    foreach (const StrutRect & rect, oldrestrictedmovearea.at(desktop))
    if (areas & rect.area())
        region += rect;
    return region;
}

QVector< QRect > Workspace::previousScreenSizes() const
{
    return oldscreensizes;
}

int Workspace::oldDisplayWidth() const
{
    return olddisplaysize.width();
}

int Workspace::oldDisplayHeight() const
{
    return olddisplaysize.height();
}

/*!
  Client \a c is moved around to position \a pos. This gives the
  workspace the opportunity to interveniate and to implement
  snap-to-windows functionality.

  The parameter \a snapAdjust is a multiplier used to calculate the
  effective snap zones. When 1.0, it means that the snap zones will be
  used without change.
 */
QPoint Workspace::adjustClientPosition(AbstractClient* c, QPoint pos, bool unrestricted, double snapAdjust)
{
    QSize borderSnapZone(options->borderSnapZone(), options->borderSnapZone());
    QRect maxRect;
    int guideMaximized = MaximizeRestore;
    if (c->maximizeMode() != MaximizeRestore) {
        maxRect = clientArea(MaximizeArea, pos + c->rect().center(), c->desktop());
        QRect geo = c->geometry();
        if (c->maximizeMode() & MaximizeHorizontal && (geo.x() == maxRect.left() || geo.right() == maxRect.right())) {
            guideMaximized |= MaximizeHorizontal;
            borderSnapZone.setWidth(qMax(borderSnapZone.width() + 2, maxRect.width() / 16));
        }
        if (c->maximizeMode() & MaximizeVertical && (geo.y() == maxRect.top() || geo.bottom() == maxRect.bottom())) {
            guideMaximized |= MaximizeVertical;
            borderSnapZone.setHeight(qMax(borderSnapZone.height() + 2, maxRect.height() / 16));
        }
    }

    if (options->windowSnapZone() || !borderSnapZone.isNull() || options->centerSnapZone()) {

        const bool sOWO = options->isSnapOnlyWhenOverlapping();
        const int screen = screens()->number(pos + c->rect().center());
        if (maxRect.isNull())
            maxRect = clientArea(MovementArea, screen, c->desktop());
        const int xmin = maxRect.left();
        const int xmax = maxRect.right() + 1;             //desk size
        const int ymin = maxRect.top();
        const int ymax = maxRect.bottom() + 1;

        const int cx(pos.x());
        const int cy(pos.y());
        const int cw(c->width());
        const int ch(c->height());
        const int rx(cx + cw);
        const int ry(cy + ch);               //these don't change

        int nx(cx), ny(cy);                         //buffers
        int deltaX(xmax);
        int deltaY(ymax);   //minimum distance to other clients

        int lx, ly, lrx, lry; //coords and size for the comparison client, l

        // border snap
        const int snapX = borderSnapZone.width() * snapAdjust; //snap trigger
        const int snapY = borderSnapZone.height() * snapAdjust;
        if (snapX || snapY) {
            QRect geo = c->geometry();
            const QPoint cp = c->clientPos();
            const QSize cs = geo.size() - c->clientSize();
            int padding[4] = { cp.x(), cs.width() - cp.x(), cp.y(), cs.height() - cp.y() };

            // snap to titlebar / snap to window borders on inner screen edges
            Client::Position titlePos = c->titlebarPosition();
            if (padding[0] && (titlePos == Client::PositionLeft || (c->maximizeMode() & MaximizeHorizontal) ||
                               screens()->intersecting(geo.translated(maxRect.x() - (padding[0] + geo.x()), 0)) > 1))
                padding[0] = 0;
            if (padding[1] && (titlePos == Client::PositionRight || (c->maximizeMode() & MaximizeHorizontal) ||
                               screens()->intersecting(geo.translated(maxRect.right() + padding[1] - geo.right(), 0)) > 1))
                padding[1] = 0;
            if (padding[2] && (titlePos == Client::PositionTop || (c->maximizeMode() & MaximizeVertical) ||
                               screens()->intersecting(geo.translated(0, maxRect.y() - (padding[2] + geo.y()))) > 1))
                padding[2] = 0;
            if (padding[3] && (titlePos == Client::PositionBottom || (c->maximizeMode() & MaximizeVertical) ||
                               screens()->intersecting(geo.translated(0, maxRect.bottom() + padding[3] - geo.bottom())) > 1))
                padding[3] = 0;
            if ((sOWO ? (cx < xmin) : true) && (qAbs(xmin - cx) < snapX)) {
                deltaX = xmin - cx;
                nx = xmin - padding[0];
            }
            if ((sOWO ? (rx > xmax) : true) && (qAbs(rx - xmax) < snapX) && (qAbs(xmax - rx) < deltaX)) {
                deltaX = rx - xmax;
                nx = xmax - cw + padding[1];
            }

            if ((sOWO ? (cy < ymin) : true) && (qAbs(ymin - cy) < snapY)) {
                deltaY = ymin - cy;
                ny = ymin - padding[2];
            }
            if ((sOWO ? (ry > ymax) : true) && (qAbs(ry - ymax) < snapY) && (qAbs(ymax - ry) < deltaY)) {
                deltaY = ry - ymax;
                ny = ymax - ch + padding[3];
            }
        }

        // windows snap
        int snap = options->windowSnapZone() * snapAdjust;
        if (snap) {
            for (auto l = m_allClients.constBegin(); l != m_allClients.constEnd(); ++l) {
                if ((*l) == c)
                    continue;
                if ((*l)->isMinimized())
                    continue; // is minimized
                if (!(*l)->isShown(false))
                    continue;
                if ((*l)->tabGroup() && (*l) != (*l)->tabGroup()->current())
                    continue; // is not active tab
                if (!((*l)->isOnDesktop(c->desktop()) || c->isOnDesktop((*l)->desktop())))
                    continue; // wrong virtual desktop
                if (!(*l)->isOnCurrentActivity())
                    continue; // wrong activity
                if ((*l)->isDesktop() || (*l)->isSplash())
                    continue;

                lx = (*l)->x();
                ly = (*l)->y();
                lrx = lx + (*l)->width();
                lry = ly + (*l)->height();

                if (!(guideMaximized & MaximizeHorizontal) &&
                    (((cy <= lry) && (cy  >= ly)) || ((ry >= ly) && (ry  <= lry)) || ((cy <= ly) && (ry >= lry)))) {
                    if ((sOWO ? (cx < lrx) : true) && (qAbs(lrx - cx) < snap) && (qAbs(lrx - cx) < deltaX)) {
                        deltaX = qAbs(lrx - cx);
                        nx = lrx;
                    }
                    if ((sOWO ? (rx > lx) : true) && (qAbs(rx - lx) < snap) && (qAbs(rx - lx) < deltaX)) {
                        deltaX = qAbs(rx - lx);
                        nx = lx - cw;
                    }
                }

                if (!(guideMaximized & MaximizeVertical) &&
                    (((cx <= lrx) && (cx  >= lx)) || ((rx >= lx) && (rx  <= lrx)) || ((cx <= lx) && (rx >= lrx)))) {
                    if ((sOWO ? (cy < lry) : true) && (qAbs(lry - cy) < snap) && (qAbs(lry - cy) < deltaY)) {
                        deltaY = qAbs(lry - cy);
                        ny = lry;
                    }
                    //if ( (qAbs( ry-ly ) < snap) && (qAbs( ry - ly ) < deltaY ))
                    if ((sOWO ? (ry > ly) : true) && (qAbs(ry - ly) < snap) && (qAbs(ry - ly) < deltaY)) {
                        deltaY = qAbs(ry - ly);
                        ny = ly - ch;
                    }
                }

                // Corner snapping
                if (!(guideMaximized & MaximizeVertical) && (nx == lrx || nx + cw == lx)) {
                    if ((sOWO ? (ry > lry) : true) && (qAbs(lry - ry) < snap) && (qAbs(lry - ry) < deltaY)) {
                        deltaY = qAbs(lry - ry);
                        ny = lry - ch;
                    }
                    if ((sOWO ? (cy < ly) : true) && (qAbs(cy - ly) < snap) && (qAbs(cy - ly) < deltaY)) {
                        deltaY = qAbs(cy - ly);
                        ny = ly;
                    }
                }
                if (!(guideMaximized & MaximizeHorizontal) && (ny == lry || ny + ch == ly)) {
                    if ((sOWO ? (rx > lrx) : true) && (qAbs(lrx - rx) < snap) && (qAbs(lrx - rx) < deltaX)) {
                        deltaX = qAbs(lrx - rx);
                        nx = lrx - cw;
                    }
                    if ((sOWO ? (cx < lx) : true) && (qAbs(cx - lx) < snap) && (qAbs(cx - lx) < deltaX)) {
                        deltaX = qAbs(cx - lx);
                        nx = lx;
                    }
                }
            }
        }

        // center snap
        snap = options->centerSnapZone() * snapAdjust; //snap trigger
        if (snap) {
            int diffX = qAbs((xmin + xmax) / 2 - (cx + cw / 2));
            int diffY = qAbs((ymin + ymax) / 2 - (cy + ch / 2));
            if (diffX < snap && diffY < snap && diffX < deltaX && diffY < deltaY) {
                // Snap to center of screen
                nx = (xmin + xmax) / 2 - cw / 2;
                ny = (ymin + ymax) / 2 - ch / 2;
            } else if (options->borderSnapZone()) {
                // Enhance border snap
                if ((nx == xmin || nx == xmax - cw) && diffY < snap && diffY < deltaY) {
                    // Snap to vertical center on screen edge
                    ny = (ymin + ymax) / 2 - ch / 2;
                } else if (((unrestricted ? ny == ymin : ny <= ymin) || ny == ymax - ch) &&
                          diffX < snap && diffX < deltaX) {
                    // Snap to horizontal center on screen edge
                    nx = (xmin + xmax) / 2 - cw / 2;
                }
            }
        }

        pos = QPoint(nx, ny);
    }
    return pos;
}

QRect Workspace::adjustClientSize(AbstractClient* c, QRect moveResizeGeom, int mode)
{
    //adapted from adjustClientPosition on 29May2004
    //this function is called when resizing a window and will modify
    //the new dimensions to snap to other windows/borders if appropriate
    if (options->windowSnapZone() || options->borderSnapZone()) {  // || options->centerSnapZone )
        const bool sOWO = options->isSnapOnlyWhenOverlapping();

        const QRect maxRect = clientArea(MovementArea, c->rect().center(), c->desktop());
        const int xmin = maxRect.left();
        const int xmax = maxRect.right();               //desk size
        const int ymin = maxRect.top();
        const int ymax = maxRect.bottom();

        const int cx(moveResizeGeom.left());
        const int cy(moveResizeGeom.top());
        const int rx(moveResizeGeom.right());
        const int ry(moveResizeGeom.bottom());

        int newcx(cx), newcy(cy);                         //buffers
        int newrx(rx), newry(ry);
        int deltaX(xmax);
        int deltaY(ymax);   //minimum distance to other clients

        int lx, ly, lrx, lry; //coords and size for the comparison client, l

        // border snap
        int snap = options->borderSnapZone(); //snap trigger
        if (snap) {
            deltaX = int(snap);
            deltaY = int(snap);

#define SNAP_BORDER_TOP \
    if ((sOWO?(newcy<ymin):true) && (qAbs(ymin-newcy)<deltaY)) \
    { \
        deltaY = qAbs(ymin-newcy); \
        newcy = ymin; \
    }

#define SNAP_BORDER_BOTTOM \
    if ((sOWO?(newry>ymax):true) && (qAbs(ymax-newry)<deltaY)) \
    { \
        deltaY = qAbs(ymax-newcy); \
        newry = ymax; \
    }

#define SNAP_BORDER_LEFT \
    if ((sOWO?(newcx<xmin):true) && (qAbs(xmin-newcx)<deltaX)) \
    { \
        deltaX = qAbs(xmin-newcx); \
        newcx = xmin; \
    }

#define SNAP_BORDER_RIGHT \
    if ((sOWO?(newrx>xmax):true) && (qAbs(xmax-newrx)<deltaX)) \
    { \
        deltaX = qAbs(xmax-newrx); \
        newrx = xmax; \
    }
            switch(mode) {
            case Client::PositionBottomRight:
                SNAP_BORDER_BOTTOM
                SNAP_BORDER_RIGHT
                break;
            case Client::PositionRight:
                SNAP_BORDER_RIGHT
                break;
            case Client::PositionBottom:
                SNAP_BORDER_BOTTOM
                break;
            case Client::PositionTopLeft:
                SNAP_BORDER_TOP
                SNAP_BORDER_LEFT
                break;
            case Client::PositionLeft:
                SNAP_BORDER_LEFT
                break;
            case Client::PositionTop:
                SNAP_BORDER_TOP
                break;
            case Client::PositionTopRight:
                SNAP_BORDER_TOP
                SNAP_BORDER_RIGHT
                break;
            case Client::PositionBottomLeft:
                SNAP_BORDER_BOTTOM
                SNAP_BORDER_LEFT
                break;
            default:
                abort();
                break;
            }


        }

        // windows snap
        snap = options->windowSnapZone();
        if (snap) {
            deltaX = int(snap);
            deltaY = int(snap);
            for (auto l = m_allClients.constBegin(); l != m_allClients.constEnd(); ++l) {
                if ((*l)->isOnDesktop(VirtualDesktopManager::self()->current()) &&
                        !(*l)->isMinimized()
                        && (*l) != c) {
                    lx = (*l)->x() - 1;
                    ly = (*l)->y() - 1;
                    lrx = (*l)->x() + (*l)->width();
                    lry = (*l)->y() + (*l)->height();

#define WITHIN_HEIGHT ((( newcy <= lry ) && ( newcy  >= ly  ))  || \
                       (( newry >= ly  ) && ( newry  <= lry ))  || \
                       (( newcy <= ly  ) && ( newry >= lry  )) )

#define WITHIN_WIDTH  ( (( cx <= lrx ) && ( cx  >= lx  ))  || \
                        (( rx >= lx  ) && ( rx  <= lrx ))  || \
                        (( cx <= lx  ) && ( rx >= lrx  )) )

#define SNAP_WINDOW_TOP  if ( (sOWO?(newcy<lry):true) \
                              && WITHIN_WIDTH  \
                              && (qAbs( lry - newcy ) < deltaY) ) {  \
    deltaY = qAbs( lry - newcy ); \
    newcy=lry; \
}

#define SNAP_WINDOW_BOTTOM  if ( (sOWO?(newry>ly):true)  \
                                 && WITHIN_WIDTH  \
                                 && (qAbs( ly - newry ) < deltaY) ) {  \
    deltaY = qAbs( ly - newry );  \
    newry=ly;  \
}

#define SNAP_WINDOW_LEFT  if ( (sOWO?(newcx<lrx):true)  \
                               && WITHIN_HEIGHT  \
                               && (qAbs( lrx - newcx ) < deltaX)) {  \
    deltaX = qAbs( lrx - newcx );  \
    newcx=lrx;  \
}

#define SNAP_WINDOW_RIGHT  if ( (sOWO?(newrx>lx):true)  \
                                && WITHIN_HEIGHT  \
                                && (qAbs( lx - newrx ) < deltaX))  \
{  \
    deltaX = qAbs( lx - newrx );  \
    newrx=lx;  \
}

#define SNAP_WINDOW_C_TOP  if ( (sOWO?(newcy<ly):true)  \
                                && (newcx == lrx || newrx == lx)  \
                                && qAbs(ly-newcy) < deltaY ) {  \
    deltaY = qAbs( ly - newcy + 1 ); \
    newcy = ly + 1; \
}

#define SNAP_WINDOW_C_BOTTOM  if ( (sOWO?(newry>lry):true)  \
                                   && (newcx == lrx || newrx == lx)  \
                                   && qAbs(lry-newry) < deltaY ) {  \
    deltaY = qAbs( lry - newry - 1 ); \
    newry = lry - 1; \
}

#define SNAP_WINDOW_C_LEFT  if ( (sOWO?(newcx<lx):true)  \
                                 && (newcy == lry || newry == ly)  \
                                 && qAbs(lx-newcx) < deltaX ) {  \
    deltaX = qAbs( lx - newcx + 1 ); \
    newcx = lx + 1; \
}

#define SNAP_WINDOW_C_RIGHT  if ( (sOWO?(newrx>lrx):true)  \
                                  && (newcy == lry || newry == ly)  \
                                  && qAbs(lrx-newrx) < deltaX ) {  \
    deltaX = qAbs( lrx - newrx - 1 ); \
    newrx = lrx - 1; \
}

                    switch(mode) {
                    case Client::PositionBottomRight:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_BOTTOM
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case Client::PositionRight:
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case Client::PositionBottom:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_C_BOTTOM
                        break;
                    case Client::PositionTopLeft:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_TOP
                        SNAP_WINDOW_C_LEFT
                        break;
                    case Client::PositionLeft:
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_LEFT
                        break;
                    case Client::PositionTop:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_C_TOP
                        break;
                    case Client::PositionTopRight:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_TOP
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case Client::PositionBottomLeft:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_BOTTOM
                        SNAP_WINDOW_C_LEFT
                        break;
                    default:
                        abort();
                        break;
                    }
                }
            }
        }

        // center snap
        //snap = options->centerSnapZone;
        //if (snap)
        //    {
        //    // Don't resize snap to center as it interferes too much
        //    // There are two ways of implementing this if wanted:
        //    // 1) Snap only to the same points that the move snap does, and
        //    // 2) Snap to the horizontal and vertical center lines of the screen
        //    }

        moveResizeGeom = QRect(QPoint(newcx, newcy), QPoint(newrx, newry));
    }
    return moveResizeGeom;
}

/*!
  Marks the client as being moved around by the user.
 */
void Workspace::setClientIsMoving(AbstractClient *c)
{
    Q_ASSERT(!c || !movingClient); // Catch attempts to move a second
    // window while still moving the first one.
    movingClient = c;
    if (movingClient)
        ++block_focus;
    else
        --block_focus;
}

// When kwin crashes, windows will not be gravitated back to their original position
// and will remain offset by the size of the decoration. So when restarting, fix this
// (the property with the size of the frame remains on the window after the crash).
void Workspace::fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geometry)
{
    NETWinInfo i(connection(), w, rootWindow(), NET::WMFrameExtents, 0);
    NETStrut frame = i.frameExtents();

    if (frame.left != 0 || frame.top != 0) {
        // left and top needed due to narrowing conversations restrictions in C++11
        const uint32_t left = frame.left;
        const uint32_t top = frame.top;
        const uint32_t values[] = { geometry->x - left, geometry->y - top };
        xcb_configure_window(connection(), w, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    }
}

//********************************************
// Client
//********************************************

/*!
  Returns \a area with the client's strut taken into account.

  Used from Workspace in updateClientArea.
 */
// TODO move to Workspace?

QRect Client::adjustedClientArea(const QRect &desktopArea, const QRect& area) const
{
    QRect r = area;
    NETExtendedStrut str = strut();
    QRect stareaL = QRect(
                        0,
                        str . left_start,
                        str . left_width,
                        str . left_end - str . left_start + 1);
    QRect stareaR = QRect(
                        desktopArea . right() - str . right_width + 1,
                        str . right_start,
                        str . right_width,
                        str . right_end - str . right_start + 1);
    QRect stareaT = QRect(
                        str . top_start,
                        0,
                        str . top_end - str . top_start + 1,
                        str . top_width);
    QRect stareaB = QRect(
                        str . bottom_start,
                        desktopArea . bottom() - str . bottom_width + 1,
                        str . bottom_end - str . bottom_start + 1,
                        str . bottom_width);

    QRect screenarea = workspace()->clientArea(ScreenArea, this);
    // HACK: workarea handling is not xinerama aware, so if this strut
    // reserves place at a xinerama edge that's inside the virtual screen,
    // ignore the strut for workspace setting.
    if (area == QRect(QPoint(0, 0), screens()->displaySize())) {
        if (stareaL.left() < screenarea.left())
            stareaL = QRect();
        if (stareaR.right() > screenarea.right())
            stareaR = QRect();
        if (stareaT.top() < screenarea.top())
            stareaT = QRect();
        if (stareaB.bottom() < screenarea.bottom())
            stareaB = QRect();
    }
    // Handle struts at xinerama edges that are inside the virtual screen.
    // They're given in virtual screen coordinates, make them affect only
    // their xinerama screen.
    stareaL.setLeft(qMax(stareaL.left(), screenarea.left()));
    stareaR.setRight(qMin(stareaR.right(), screenarea.right()));
    stareaT.setTop(qMax(stareaT.top(), screenarea.top()));
    stareaB.setBottom(qMin(stareaB.bottom(), screenarea.bottom()));

    if (stareaL . intersects(area)) {
//        qDebug() << "Moving left of: " << r << " to " << stareaL.right() + 1;
        r . setLeft(stareaL . right() + 1);
    }
    if (stareaR . intersects(area)) {
//        qDebug() << "Moving right of: " << r << " to " << stareaR.left() - 1;
        r . setRight(stareaR . left() - 1);
    }
    if (stareaT . intersects(area)) {
//        qDebug() << "Moving top of: " << r << " to " << stareaT.bottom() + 1;
        r . setTop(stareaT . bottom() + 1);
    }
    if (stareaB . intersects(area)) {
//        qDebug() << "Moving bottom of: " << r << " to " << stareaB.top() - 1;
        r . setBottom(stareaB . top() - 1);
    }

    return r;
}

NETExtendedStrut Client::strut() const
{
    NETExtendedStrut ext = info->extendedStrut();
    NETStrut str = info->strut();
    const QSize displaySize = screens()->displaySize();
    if (ext.left_width == 0 && ext.right_width == 0 && ext.top_width == 0 && ext.bottom_width == 0
            && (str.left != 0 || str.right != 0 || str.top != 0 || str.bottom != 0)) {
        // build extended from simple
        if (str.left != 0) {
            ext.left_width = str.left;
            ext.left_start = 0;
            ext.left_end = displaySize.height();
        }
        if (str.right != 0) {
            ext.right_width = str.right;
            ext.right_start = 0;
            ext.right_end = displaySize.height();
        }
        if (str.top != 0) {
            ext.top_width = str.top;
            ext.top_start = 0;
            ext.top_end = displaySize.width();
        }
        if (str.bottom != 0) {
            ext.bottom_width = str.bottom;
            ext.bottom_start = 0;
            ext.bottom_end = displaySize.width();
        }
    }
    return ext;
}

StrutRect Client::strutRect(StrutArea area) const
{
    assert(area != StrutAreaAll);   // Not valid
    const QSize displaySize = screens()->displaySize();
    NETExtendedStrut strutArea = strut();
    switch(area) {
    case StrutAreaTop:
        if (strutArea.top_width != 0)
            return StrutRect(QRect(
                                 strutArea.top_start, 0,
                                 strutArea.top_end - strutArea.top_start, strutArea.top_width
                             ), StrutAreaTop);
        break;
    case StrutAreaRight:
        if (strutArea.right_width != 0)
            return StrutRect(QRect(
                                 displaySize.width() - strutArea.right_width, strutArea.right_start,
                                 strutArea.right_width, strutArea.right_end - strutArea.right_start
                             ), StrutAreaRight);
        break;
    case StrutAreaBottom:
        if (strutArea.bottom_width != 0)
            return StrutRect(QRect(
                                 strutArea.bottom_start, displaySize.height() - strutArea.bottom_width,
                                 strutArea.bottom_end - strutArea.bottom_start, strutArea.bottom_width
                             ), StrutAreaBottom);
        break;
    case StrutAreaLeft:
        if (strutArea.left_width != 0)
            return StrutRect(QRect(
                                 0, strutArea.left_start,
                                 strutArea.left_width, strutArea.left_end - strutArea.left_start
                             ), StrutAreaLeft);
        break;
    default:
        abort(); // Not valid
    }
    return StrutRect(); // Null rect
}

StrutRects Client::strutRects() const
{
    StrutRects region;
    region += strutRect(StrutAreaTop);
    region += strutRect(StrutAreaRight);
    region += strutRect(StrutAreaBottom);
    region += strutRect(StrutAreaLeft);
    return region;
}

bool Client::hasStrut() const
{
    NETExtendedStrut ext = strut();
    if (ext.left_width == 0 && ext.right_width == 0 && ext.top_width == 0 && ext.bottom_width == 0)
        return false;
    return true;
}

bool Client::hasOffscreenXineramaStrut() const
{
    // Get strut as a QRegion
    QRegion region;
    region += strutRect(StrutAreaTop);
    region += strutRect(StrutAreaRight);
    region += strutRect(StrutAreaBottom);
    region += strutRect(StrutAreaLeft);

    // Remove all visible areas so that only the invisible remain
    for (int i = 0; i < screens()->count(); i ++)
        region -= screens()->geometry(i);

    // If there's anything left then we have an offscreen strut
    return !region.isEmpty();
}

void AbstractClient::checkWorkspacePosition(QRect oldGeometry, int oldDesktop, QRect oldClientGeometry)
{
    enum { Left = 0, Top, Right, Bottom };
    const int border[4] = { borderLeft(), borderTop(), borderRight(), borderBottom() };
    if( !oldGeometry.isValid())
        oldGeometry = geometry();
    if( oldDesktop == -2 )
        oldDesktop = desktop();
    if (!oldClientGeometry.isValid())
        oldClientGeometry = oldGeometry.adjusted(border[Left], border[Top], -border[Right], -border[Bottom]);
    if (isDesktop())
        return;
    if (isFullScreen()) {
        QRect area = workspace()->clientArea(FullScreenArea, this);
        if (geometry() != area)
            setGeometry(area);
        return;
    }
    if (isDock())
        return;

    if (maximizeMode() != MaximizeRestore) {
        // TODO update geom_restore?
        changeMaximize(false, false, true);   // adjust size
        const QRect screenArea = workspace()->clientArea(ScreenArea, this);
        QRect geom = geometry();
        checkOffscreenPosition(&geom, screenArea);
        setGeometry(geom);
        return;
    }

    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        setGeometry(electricBorderMaximizeGeometry(geometry().center(), desktop()));
        return;
    }

    // this can be true only if this window was mapped before KWin
    // was started - in such case, don't adjust position to workarea,
    // because the window already had its position, and if a window
    // with a strut altering the workarea would be managed in initialization
    // after this one, this window would be moved
    if (!workspace() || workspace()->initializing())
        return;

    // If the window was touching an edge before but not now move it so it is again.
    // Old and new maximums have different starting values so windows on the screen
    // edge will move when a new strut is placed on the edge.
    QRect oldScreenArea;
    QRect oldGeomTall;
    QRect oldGeomWide;
    const auto displaySize = screens()->displaySize();
    if( workspace()->inUpdateClientArea()) {
        // we need to find the screen area as it was before the change
        oldScreenArea = QRect( 0, 0, workspace()->oldDisplayWidth(), workspace()->oldDisplayHeight());
        oldGeomTall = QRect(oldGeometry.x(), 0, oldGeometry.width(), workspace()->oldDisplayHeight());   // Full screen height
        oldGeomWide = QRect(0, oldGeometry.y(), workspace()->oldDisplayWidth(), oldGeometry.height());   // Full screen width
        int distance = INT_MAX;
        foreach(const QRect &r, workspace()->previousScreenSizes()) {
            int d = r.contains( oldGeometry.center()) ? 0 : ( r.center() - oldGeometry.center()).manhattanLength();
            if( d < distance ) {
                distance = d;
                oldScreenArea = r;
            }
        }
    } else {
        oldScreenArea = workspace()->clientArea(ScreenArea, oldGeometry.center(), oldDesktop);
        oldGeomTall = QRect(oldGeometry.x(), 0, oldGeometry.width(), displaySize.height());   // Full screen height
        oldGeomWide = QRect(0, oldGeometry.y(), displaySize.width(), oldGeometry.height());   // Full screen width
    }
    int oldTopMax = oldScreenArea.y();
    int oldRightMax = oldScreenArea.x() + oldScreenArea.width();
    int oldBottomMax = oldScreenArea.y() + oldScreenArea.height();
    int oldLeftMax = oldScreenArea.x();
    const QRect screenArea = workspace()->clientArea(ScreenArea, geometryRestore().center(), desktop());
    int topMax = screenArea.y();
    int rightMax = screenArea.x() + screenArea.width();
    int bottomMax = screenArea.y() + screenArea.height();
    int leftMax = screenArea.x();
    QRect newGeom = geometryRestore(); // geometry();
    QRect newClientGeom = newGeom.adjusted(border[Left], border[Top], -border[Right], -border[Bottom]);
    const QRect newGeomTall = QRect(newGeom.x(), 0, newGeom.width(), displaySize.height());   // Full screen height
    const QRect newGeomWide = QRect(0, newGeom.y(), displaySize.width(), newGeom.height());   // Full screen width
    // Get the max strut point for each side where the window is (E.g. Highest point for
    // the bottom struts bounded by the window's left and right sides).

    // These 4 compute old bounds ...
    auto moveAreaFunc = workspace()->inUpdateClientArea() ?
                                &Workspace::previousRestrictedMoveArea : //... the restricted areas changed
                                &Workspace::restrictedMoveArea; //... when e.g. active desktop or screen changes

    foreach (const QRect & r, (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaTop).rects()) {
        QRect rect = r & oldGeomTall;
        if (!rect.isEmpty())
            oldTopMax = qMax(oldTopMax, rect.y() + rect.height());
    }
    foreach (const QRect & r, (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaRight).rects()) {
        QRect rect = r & oldGeomWide;
        if (!rect.isEmpty())
            oldRightMax = qMin(oldRightMax, rect.x());
    }
    foreach (const QRect & r, (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaBottom).rects()) {
        QRect rect = r & oldGeomTall;
        if (!rect.isEmpty())
            oldBottomMax = qMin(oldBottomMax, rect.y());
    }
    foreach (const QRect & r, (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaLeft).rects()) {
        QRect rect = r & oldGeomWide;
        if (!rect.isEmpty())
            oldLeftMax = qMax(oldLeftMax, rect.x() + rect.width());
    }

    // These 4 compute new bounds
    foreach (const QRect & r, workspace()->restrictedMoveArea(desktop(), StrutAreaTop).rects()) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty())
            topMax = qMax(topMax, rect.y() + rect.height());
    }
    foreach (const QRect & r, workspace()->restrictedMoveArea(desktop(), StrutAreaRight).rects()) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty())
            rightMax = qMin(rightMax, rect.x());
    }
    foreach (const QRect & r, workspace()->restrictedMoveArea(desktop(), StrutAreaBottom).rects()) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty())
            bottomMax = qMin(bottomMax, rect.y());
    }
    foreach (const QRect & r, workspace()->restrictedMoveArea(desktop(), StrutAreaLeft).rects()) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty())
            leftMax = qMax(leftMax, rect.x() + rect.width());
    }


    // Check if the sides were inside or touching but are no longer
    bool keep[4] = {false, false, false, false};
    bool save[4] = {false, false, false, false};
    int padding[4] = {0, 0, 0, 0};
    if (oldGeometry.x() >= oldLeftMax)
        save[Left] = newGeom.x() < leftMax;
    if (oldGeometry.x() == oldLeftMax)
        keep[Left] = newGeom.x() != leftMax;
    else if (oldClientGeometry.x() == oldLeftMax && newClientGeom.x() != leftMax) {
        padding[0] = border[Left];
        keep[Left] = true;
    }
    if (oldGeometry.y() >= oldTopMax)
        save[Top] = newGeom.y() < topMax;
    if (oldGeometry.y() == oldTopMax)
        keep[Top] = newGeom.y() != topMax;
    else if (oldClientGeometry.y() == oldTopMax && newClientGeom.y() != topMax) {
        padding[1] = border[Left];
        keep[Top] = true;
    }
    if (oldGeometry.right() <= oldRightMax - 1)
        save[Right] = newGeom.right() > rightMax - 1;
    if (oldGeometry.right() == oldRightMax - 1)
        keep[Right] = newGeom.right() != rightMax - 1;
    else if (oldClientGeometry.right() == oldRightMax - 1 && newClientGeom.right() != rightMax - 1) {
        padding[2] = border[Right];
        keep[Right] = true;
    }
    if (oldGeometry.bottom() <= oldBottomMax - 1)
        save[Bottom] = newGeom.bottom() > bottomMax - 1;
    if (oldGeometry.bottom() == oldBottomMax - 1)
        keep[Bottom] = newGeom.bottom() != bottomMax - 1;
    else if (oldClientGeometry.bottom() == oldBottomMax - 1 && newClientGeom.bottom() != bottomMax - 1) {
        padding[3] = border[Bottom];
        keep[Bottom] = true;
    }

    // if randomly touches opposing edges, do not favor either
    if (keep[Left] && keep[Right]) {
        keep[Left] = keep[Right] = false;
        padding[0] = padding[2] = 0;
    }
    if (keep[Top] && keep[Bottom]) {
        keep[Top] = keep[Bottom] = false;
        padding[1] = padding[3] = 0;
    }

    if (save[Left] || keep[Left])
        newGeom.moveLeft(qMax(leftMax, screenArea.x()) - padding[0]);
    if (padding[0] && screens()->intersecting(newGeom) > 1)
        newGeom.moveLeft(newGeom.left() + padding[0]);
    if (save[Top] || keep[Top])
        newGeom.moveTop(qMax(topMax, screenArea.y()) - padding[1]);
    if (padding[1] && screens()->intersecting(newGeom) > 1)
        newGeom.moveTop(newGeom.top() + padding[1]);
    if (save[Right] || keep[Right])
        newGeom.moveRight(qMin(rightMax - 1, screenArea.right()) + padding[2]);
    if (padding[2] && screens()->intersecting(newGeom) > 1)
        newGeom.moveRight(newGeom.right() - padding[2]);
    if (oldGeometry.x() >= oldLeftMax && newGeom.x() < leftMax)
        newGeom.setLeft(qMax(leftMax, screenArea.x()));
    else if (oldClientGeometry.x() >= oldLeftMax && newGeom.x() + border[Left] < leftMax) {
        newGeom.setLeft(qMax(leftMax, screenArea.x()) - border[Left]);
        if (screens()->intersecting(newGeom) > 1)
            newGeom.setLeft(newGeom.left() + border[Left]);
    }
    if (save[Bottom] || keep[Bottom])
        newGeom.moveBottom(qMin(bottomMax - 1, screenArea.bottom()) + padding[3]);
    if (padding[3] && screens()->intersecting(newGeom) > 1)
        newGeom.moveBottom(newGeom.bottom() - padding[3]);
    if (oldGeometry.y() >= oldTopMax && newGeom.y() < topMax)
        newGeom.setTop(qMax(topMax, screenArea.y()));
    else if (oldClientGeometry.y() >= oldTopMax && newGeom.y() + border[Top] < topMax) {
        newGeom.setTop(qMax(topMax, screenArea.y()) - border[Top]);
        if (screens()->intersecting(newGeom) > 1)
            newGeom.setTop(newGeom.top() + border[Top]);
    }

    checkOffscreenPosition(&newGeom, screenArea);
    // Obey size hints. TODO: We really should make sure it stays in the right place
    if (!isShade())
        newGeom.setSize(adjustedSize(newGeom.size()));

    if (newGeom != geometry())
        setGeometry(newGeom);
}

void AbstractClient::checkOffscreenPosition(QRect* geom, const QRect& screenArea)
{
    if (geom->left() > screenArea.right()) {
        geom->moveLeft(screenArea.right() - screenArea.width()/4);
    } else if (geom->right() < screenArea.left()) {
        geom->moveRight(screenArea.left() + screenArea.width()/4);
    }
    if (geom->top() > screenArea.bottom()) {
        geom->moveTop(screenArea.bottom() - screenArea.height()/4);
    } else if (geom->bottom() < screenArea.top()) {
        geom->moveBottom(screenArea.top() + screenArea.width()/4);
    }
}

/*!
  Adjust the frame size \a frame according to he window's size hints.
 */
QSize AbstractClient::adjustedSize(const QSize& frame, Sizemode mode) const
{
    // first, get the window size for the given frame size s
    QSize wsize(frame.width() - (borderLeft() + borderRight()),
                frame.height() - (borderTop() + borderBottom()));
    if (wsize.isEmpty())
        wsize = QSize(qMax(wsize.width(), 1), qMax(wsize.height(), 1));

    return sizeForClientSize(wsize, mode, false);
}

// this helper returns proper size even if the window is shaded
// see also the comment in Client::setGeometry()
QSize AbstractClient::adjustedSize() const
{
    return sizeForClientSize(clientSize());
}

/*!
  Calculate the appropriate frame size for the given client size \a
  wsize.

  \a wsize is adapted according to the window's size hints (minimum,
  maximum and incremental size changes).

 */
QSize Client::sizeForClientSize(const QSize& wsize, Sizemode mode, bool noframe) const
{
    int w = wsize.width();
    int h = wsize.height();
    if (w < 1 || h < 1) {
        qCWarning(KWIN_CORE) << "sizeForClientSize() with empty size!" ;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    // basesize, minsize, maxsize, paspect and resizeinc have all values defined,
    // even if they're not set in flags - see getWmNormalHints()
    QSize min_size = tabGroup() ? tabGroup()->minSize() : minSize();
    QSize max_size = tabGroup() ? tabGroup()->maxSize() : maxSize();
    if (isDecorated()) {
        QSize decominsize(0, 0);
        QSize border_size(borderLeft() + borderRight(), borderTop() + borderBottom());
        if (border_size.width() > decominsize.width())  // just in case
            decominsize.setWidth(border_size.width());
        if (border_size.height() > decominsize.height())
            decominsize.setHeight(border_size.height());
        if (decominsize.width() > min_size.width())
            min_size.setWidth(decominsize.width());
        if (decominsize.height() > min_size.height())
            min_size.setHeight(decominsize.height());
    }
    w = qMin(max_size.width(), w);
    h = qMin(max_size.height(), h);
    w = qMax(min_size.width(), w);
    h = qMax(min_size.height(), h);

    int w1 = w;
    int h1 = h;
    int width_inc = m_geometryHints.resizeIncrements().width();
    int height_inc = m_geometryHints.resizeIncrements().height();
    int basew_inc = m_geometryHints.baseSize().width();
    int baseh_inc = m_geometryHints.baseSize().height();
    if (!m_geometryHints.hasBaseSize()) {
        basew_inc = m_geometryHints.minSize().width();
        baseh_inc = m_geometryHints.minSize().height();
    }
    w = int((w - basew_inc) / width_inc) * width_inc + basew_inc;
    h = int((h - baseh_inc) / height_inc) * height_inc + baseh_inc;
// code for aspect ratios based on code from FVWM
    /*
     * The math looks like this:
     *
     * minAspectX    dwidth     maxAspectX
     * ---------- <= ------- <= ----------
     * minAspectY    dheight    maxAspectY
     *
     * If that is multiplied out, then the width and height are
     * invalid in the following situations:
     *
     * minAspectX * dheight > minAspectY * dwidth
     * maxAspectX * dheight < maxAspectY * dwidth
     *
     */
    if (m_geometryHints.hasAspect()) {
        double min_aspect_w = m_geometryHints.minAspect().width(); // use doubles, because the values can be MAX_INT
        double min_aspect_h = m_geometryHints.minAspect().height(); // and multiplying would go wrong otherwise
        double max_aspect_w = m_geometryHints.maxAspect().width();
        double max_aspect_h = m_geometryHints.maxAspect().height();
        // According to ICCCM 4.1.2.3 PMinSize should be a fallback for PBaseSize for size increments,
        // but not for aspect ratio. Since this code comes from FVWM, handles both at the same time,
        // and I have no idea how it works, let's hope nobody relies on that.
        const QSize baseSize = m_geometryHints.baseSize();
        w -= baseSize.width();
        h -= baseSize.height();
        int max_width = max_size.width() - baseSize.width();
        int min_width = min_size.width() - baseSize.width();
        int max_height = max_size.height() - baseSize.height();
        int min_height = min_size.height() - baseSize.height();
#define ASPECT_CHECK_GROW_W \
    if ( min_aspect_w * h > min_aspect_h * w ) \
    { \
        int delta = int( min_aspect_w * h / min_aspect_h - w ) / width_inc * width_inc; \
        if ( w + delta <= max_width ) \
            w += delta; \
    }
#define ASPECT_CHECK_SHRINK_H_GROW_W \
    if ( min_aspect_w * h > min_aspect_h * w ) \
    { \
        int delta = int( h - w * min_aspect_h / min_aspect_w ) / height_inc * height_inc; \
        if ( h - delta >= min_height ) \
            h -= delta; \
        else \
        { \
            int delta = int( min_aspect_w * h / min_aspect_h - w ) / width_inc * width_inc; \
            if ( w + delta <= max_width ) \
                w += delta; \
        } \
    }
#define ASPECT_CHECK_GROW_H \
    if ( max_aspect_w * h < max_aspect_h * w ) \
    { \
        int delta = int( w * max_aspect_h / max_aspect_w - h ) / height_inc * height_inc; \
        if ( h + delta <= max_height ) \
            h += delta; \
    }
#define ASPECT_CHECK_SHRINK_W_GROW_H \
    if ( max_aspect_w * h < max_aspect_h * w ) \
    { \
        int delta = int( w - max_aspect_w * h / max_aspect_h ) / width_inc * width_inc; \
        if ( w - delta >= min_width ) \
            w -= delta; \
        else \
        { \
            int delta = int( w * max_aspect_h / max_aspect_w - h ) / height_inc * height_inc; \
            if ( h + delta <= max_height ) \
                h += delta; \
        } \
    }
        switch(mode) {
        case SizemodeAny:
#if 0 // make SizemodeAny equal to SizemodeFixedW - prefer keeping fixed width,
            // so that changing aspect ratio to a different value and back keeps the same size (#87298)
            {
                ASPECT_CHECK_SHRINK_H_GROW_W
                ASPECT_CHECK_SHRINK_W_GROW_H
                ASPECT_CHECK_GROW_H
                ASPECT_CHECK_GROW_W
                break;
            }
#endif
        case SizemodeFixedW: {
            // the checks are order so that attempts to modify height are first
            ASPECT_CHECK_GROW_H
            ASPECT_CHECK_SHRINK_H_GROW_W
            ASPECT_CHECK_SHRINK_W_GROW_H
            ASPECT_CHECK_GROW_W
            break;
        }
        case SizemodeFixedH: {
            ASPECT_CHECK_GROW_W
            ASPECT_CHECK_SHRINK_W_GROW_H
            ASPECT_CHECK_SHRINK_H_GROW_W
            ASPECT_CHECK_GROW_H
            break;
        }
        case SizemodeMax: {
            // first checks that try to shrink
            ASPECT_CHECK_SHRINK_H_GROW_W
            ASPECT_CHECK_SHRINK_W_GROW_H
            ASPECT_CHECK_GROW_W
            ASPECT_CHECK_GROW_H
            break;
        }
        }
#undef ASPECT_CHECK_SHRINK_H_GROW_W
#undef ASPECT_CHECK_SHRINK_W_GROW_H
#undef ASPECT_CHECK_GROW_W
#undef ASPECT_CHECK_GROW_H
        w += baseSize.width();
        h += baseSize.height();
    }
    if (!rules()->checkStrictGeometry(!isFullScreen())) {
        // disobey increments and aspect by explicit rule
        w = w1;
        h = h1;
    }

    if (!noframe) {
        w += borderLeft() + borderRight();
        h += borderTop() + borderBottom();
    }
    return rules()->checkSize(QSize(w, h));
}

/*!
  Gets the client's normal WM hints and reconfigures itself respectively.
 */
void Client::getWmNormalHints()
{
    const bool hadFixedAspect = m_geometryHints.hasAspect();
    // roundtrip to X server
    m_geometryHints.fetch();
    m_geometryHints.read();

    if (!hadFixedAspect && m_geometryHints.hasAspect()) {
        // align to eventual new contraints
        maximize(max_mode);
    }
    // Update min/max size of this group
    if (tabGroup())
        tabGroup()->updateMinMaxSize();

    if (isManaged()) {
        // update to match restrictions
        QSize new_size = adjustedSize();
        if (new_size != size() && !isFullScreen()) {
            QRect origClientGeometry(pos() + clientPos(), clientSize());
            resizeWithChecks(new_size);
            if ((!isSpecialWindow() || isToolbar()) && !isFullScreen()) {
                // try to keep the window in its xinerama screen if possible,
                // if that fails at least keep it visible somewhere
                QRect area = workspace()->clientArea(MovementArea, this);
                if (area.contains(origClientGeometry))
                    keepInArea(area);
                area = workspace()->clientArea(WorkArea, this);
                if (area.contains(origClientGeometry))
                    keepInArea(area);
            }
        }
    }
    updateAllowedActions(); // affects isResizeable()
}

QSize Client::minSize() const
{
    return rules()->checkMinSize(m_geometryHints.minSize());
}

QSize Client::maxSize() const
{
    return rules()->checkMaxSize(m_geometryHints.maxSize());
}

QSize Client::basicUnit() const
{
    return m_geometryHints.resizeIncrements();
}

/*!
  Auxiliary function to inform the client about the current window
  configuration.

 */
void Client::sendSyntheticConfigureNotify()
{
    xcb_configure_notify_event_t c;
    memset(&c, 0, sizeof(c));
    c.response_type = XCB_CONFIGURE_NOTIFY;
    c.event = window();
    c.window = window();
    c.x = x() + clientPos().x();
    c.y = y() + clientPos().y();
    c.width = clientSize().width();
    c.height = clientSize().height();
    c.border_width = 0;
    c.above_sibling = XCB_WINDOW_NONE;
    c.override_redirect = 0;
    xcb_send_event(connection(), true, c.event, XCB_EVENT_MASK_STRUCTURE_NOTIFY, reinterpret_cast<const char*>(&c));
    xcb_flush(connection());
}

const QPoint Client::calculateGravitation(bool invert, int gravity) const
{
    int dx, dy;
    dx = dy = 0;

    if (gravity == 0)   // default (nonsense) value for the argument
        gravity = m_geometryHints.windowGravity();

// dx, dy specify how the client window moves to make space for the frame
    switch(gravity) {
    case NorthWestGravity: // move down right
    default:
        dx = borderLeft();
        dy = borderTop();
        break;
    case NorthGravity: // move right
        dx = 0;
        dy = borderTop();
        break;
    case NorthEastGravity: // move down left
        dx = -borderRight();
        dy = borderTop();
        break;
    case WestGravity: // move right
        dx = borderLeft();
        dy = 0;
        break;
    case CenterGravity:
        break; // will be handled specially
    case StaticGravity: // don't move
        dx = 0;
        dy = 0;
        break;
    case EastGravity: // move left
        dx = -borderRight();
        dy = 0;
        break;
    case SouthWestGravity: // move up right
        dx = borderLeft() ;
        dy = -borderBottom();
        break;
    case SouthGravity: // move up
        dx = 0;
        dy = -borderBottom();
        break;
    case SouthEastGravity: // move up left
        dx = -borderRight();
        dy = -borderBottom();
        break;
    }
    if (gravity != CenterGravity) {
        // translate from client movement to frame movement
        dx -= borderLeft();
        dy -= borderTop();
    } else {
        // center of the frame will be at the same position client center without frame would be
        dx = - (borderLeft() + borderRight()) / 2;
        dy = - (borderTop() + borderBottom()) / 2;
    }
    if (!invert)
        return QPoint(x() + dx, y() + dy);
    else
        return QPoint(x() - dx, y() - dy);
}

void Client::configureRequest(int value_mask, int rx, int ry, int rw, int rh, int gravity, bool from_tool)
{
    // "maximized" is a user setting -> we do not allow the client to resize itself
    // away from this & against the users explicit wish
    qCDebug(KWIN_CORE) << this << bool(value_mask & (CWX|CWWidth|CWY|CWHeight)) <<
                            bool(maximizeMode() & MaximizeVertical) <<
                            bool(maximizeMode() & MaximizeHorizontal);

    // we want to (partially) ignore the request when the window is somehow maximized or quicktiled
    bool ignore = !app_noborder && (quickTileMode() != QuickTileMode(QuickTileFlag::None) || maximizeMode() != MaximizeRestore);
    // however, the user shall be able to force obedience despite and also disobedience in general
    ignore = rules()->checkIgnoreGeometry(ignore);
    if (!ignore) { // either we're not max'd / q'tiled or the user allowed the client to break that - so break it.
        updateQuickTileMode(QuickTileFlag::None);
        max_mode = MaximizeRestore;
        emit quickTileModeChanged();
    } else if (!app_noborder && quickTileMode() == QuickTileMode(QuickTileFlag::None) &&
        (maximizeMode() == MaximizeVertical || maximizeMode() == MaximizeHorizontal)) {
        // ignoring can be, because either we do, or the user does explicitly not want it.
        // for partially maximized windows we want to allow configures in the other dimension.
        // so we've to ask the user again - to know whether we just ignored for the partial maximization.
        // the problem here is, that the user can explicitly permit configure requests - even for maximized windows!
        // we cannot distinguish that from passing "false" for partially maximized windows.
        ignore = rules()->checkIgnoreGeometry(false);
        if (!ignore) { // the user is not interested, so we fix up dimensions
            if (maximizeMode() == MaximizeVertical)
                value_mask &= ~(CWY|CWHeight);
            if (maximizeMode() == MaximizeHorizontal)
                value_mask &= ~(CWX|CWWidth);
            if (!(value_mask & (CWX|CWWidth|CWY|CWHeight))) {
                ignore = true; // the modification turned the request void
            }
        }
    }

    if (ignore) {
        qCDebug(KWIN_CORE) << "DENIED";
        return; // nothing to (left) to do for use - bugs #158974, #252314, #321491
    }

    qCDebug(KWIN_CORE) << "PERMITTED" << this << bool(value_mask & (CWX|CWWidth|CWY|CWHeight));

    if (gravity == 0)   // default (nonsense) value for the argument
        gravity = m_geometryHints.windowGravity();
    if (value_mask & (CWX | CWY)) {
        QPoint new_pos = calculateGravitation(true, gravity);   // undo gravitation
        if (value_mask & CWX)
            new_pos.setX(rx);
        if (value_mask & CWY)
            new_pos.setY(ry);

        // clever(?) workaround for applications like xv that want to set
        // the location to the current location but miscalculate the
        // frame size due to kwin being a double-reparenting window
        // manager
        if (new_pos.x() == x() + clientPos().x() && new_pos.y() == y() + clientPos().y()
                && gravity == NorthWestGravity && !from_tool) {
            new_pos.setX(x());
            new_pos.setY(y());
        }

        int nw = clientSize().width();
        int nh = clientSize().height();
        if (value_mask & CWWidth)
            nw = rw;
        if (value_mask & CWHeight)
            nh = rh;
        QSize ns = sizeForClientSize(QSize(nw, nh));     // enforces size if needed
        new_pos = rules()->checkPosition(new_pos);
        int newScreen = screens()->number(QRect(new_pos, ns).center());
        if (newScreen != rules()->checkScreen(newScreen))
            return; // not allowed by rule

        QRect origClientGeometry(pos() + clientPos(), clientSize());
        GeometryUpdatesBlocker blocker(this);
        move(new_pos);
        plainResize(ns);
        setGeometry(QRect(calculateGravitation(false, gravity), size()));
        updateFullScreenHack(QRect(new_pos, QSize(nw, nh)));
        QRect area = workspace()->clientArea(WorkArea, this);
        if (!from_tool && (!isSpecialWindow() || isToolbar()) && !isFullScreen()
                && area.contains(origClientGeometry))
            keepInArea(area);

        // this is part of the kicker-xinerama-hack... it should be
        // safe to remove when kicker gets proper ExtendedStrut support;
        // see Workspace::updateClientArea() and
        // Client::adjustedClientArea()
        if (hasStrut())
            workspace() -> updateClientArea();
    }

    if (value_mask & (CWWidth | CWHeight)
            && !(value_mask & (CWX | CWY))) {     // pure resize
        int nw = clientSize().width();
        int nh = clientSize().height();
        if (value_mask & CWWidth)
            nw = rw;
        if (value_mask & CWHeight)
            nh = rh;
        QSize ns = sizeForClientSize(QSize(nw, nh));

        if (ns != size()) { // don't restore if some app sets its own size again
            QRect origClientGeometry(pos() + clientPos(), clientSize());
            GeometryUpdatesBlocker blocker(this);
            resizeWithChecks(ns, xcb_gravity_t(gravity));
            updateFullScreenHack(QRect(calculateGravitation(true, m_geometryHints.windowGravity()), QSize(nw, nh)));
            if (!from_tool && (!isSpecialWindow() || isToolbar()) && !isFullScreen()) {
                // try to keep the window in its xinerama screen if possible,
                // if that fails at least keep it visible somewhere
                QRect area = workspace()->clientArea(MovementArea, this);
                if (area.contains(origClientGeometry))
                    keepInArea(area);
                area = workspace()->clientArea(WorkArea, this);
                if (area.contains(origClientGeometry))
                    keepInArea(area);
            }
        }
    }
    geom_restore = geometry();
    // No need to send synthetic configure notify event here, either it's sent together
    // with geometry change, or there's no need to send it.
    // Handling of the real ConfigureRequest event forces sending it, as there it's necessary.
}

void Client::resizeWithChecks(int w, int h, xcb_gravity_t gravity, ForceGeometry_t force)
{
    assert(!shade_geometry_change);
    if (isShade()) {
        if (h == borderTop() + borderBottom()) {
            qCWarning(KWIN_CORE) << "Shaded geometry passed for size:" ;
        }
    }
    int newx = x();
    int newy = y();
    QRect area = workspace()->clientArea(WorkArea, this);
    // don't allow growing larger than workarea
    if (w > area.width())
        w = area.width();
    if (h > area.height())
        h = area.height();
    QSize tmp = adjustedSize(QSize(w, h));    // checks size constraints, including min/max size
    w = tmp.width();
    h = tmp.height();
    if (gravity == 0) {
        gravity = m_geometryHints.windowGravity();
    }
    switch(gravity) {
    case NorthWestGravity: // top left corner doesn't move
    default:
        break;
    case NorthGravity: // middle of top border doesn't move
        newx = (newx + width() / 2) - (w / 2);
        break;
    case NorthEastGravity: // top right corner doesn't move
        newx = newx + width() - w;
        break;
    case WestGravity: // middle of left border doesn't move
        newy = (newy + height() / 2) - (h / 2);
        break;
    case CenterGravity: // middle point doesn't move
        newx = (newx + width() / 2) - (w / 2);
        newy = (newy + height() / 2) - (h / 2);
        break;
    case StaticGravity: // top left corner of _client_ window doesn't move
        // since decoration doesn't change, equal to NorthWestGravity
        break;
    case EastGravity: // // middle of right border doesn't move
        newx = newx + width() - w;
        newy = (newy + height() / 2) - (h / 2);
        break;
    case SouthWestGravity: // bottom left corner doesn't move
        newy = newy + height() - h;
        break;
    case SouthGravity: // middle of bottom border doesn't move
        newx = (newx + width() / 2) - (w / 2);
        newy = newy + height() - h;
        break;
    case SouthEastGravity: // bottom right corner doesn't move
        newx = newx + width() - w;
        newy = newy + height() - h;
        break;
    }
    setGeometry(newx, newy, w, h, force);
}

// _NET_MOVERESIZE_WINDOW
void Client::NETMoveResizeWindow(int flags, int x, int y, int width, int height)
{
    int gravity = flags & 0xff;
    int value_mask = 0;
    if (flags & (1 << 8))
        value_mask |= CWX;
    if (flags & (1 << 9))
        value_mask |= CWY;
    if (flags & (1 << 10))
        value_mask |= CWWidth;
    if (flags & (1 << 11))
        value_mask |= CWHeight;
    configureRequest(value_mask, x, y, width, height, gravity, true);
}

/*!
  Returns whether the window is moveable or has a fixed
  position.
 */
bool Client::isMovable() const
{
    if (!hasNETSupport() && !m_motif.move()) {
        return false;
    }
    if (isFullScreen())
        return false;
    if (isSpecialWindow() && !isSplash() && !isToolbar())  // allow moving of splashscreens :)
        return false;
    if (rules()->checkPosition(invalidPoint) != invalidPoint)     // forced position
        return false;
    return true;
}

/*!
  Returns whether the window is moveable across Xinerama screens
 */
bool Client::isMovableAcrossScreens() const
{
    if (!hasNETSupport() && !m_motif.move()) {
        return false;
    }
    if (isSpecialWindow() && !isSplash() && !isToolbar())  // allow moving of splashscreens :)
        return false;
    if (rules()->checkPosition(invalidPoint) != invalidPoint)     // forced position
        return false;
    return true;
}

/*!
  Returns whether the window is resizable or has a fixed size.
 */
bool Client::isResizable() const
{
    if (!hasNETSupport() && !m_motif.resize()) {
        return false;
    }
    if (isFullScreen())
        return false;
    if (isSpecialWindow() || isSplash() || isToolbar())
        return false;
    if (rules()->checkSize(QSize()).isValid())   // forced size
        return false;
    const Position mode = moveResizePointerMode();
    if ((mode == PositionTop || mode == PositionTopLeft || mode == PositionTopRight ||
         mode == PositionLeft || mode == PositionBottomLeft) && rules()->checkPosition(invalidPoint) != invalidPoint)
        return false;

    QSize min = tabGroup() ? tabGroup()->minSize() : minSize();
    QSize max = tabGroup() ? tabGroup()->maxSize() : maxSize();
    return min.width() < max.width() || min.height() < max.height();
}

/*
  Returns whether the window is maximizable or not
 */
bool Client::isMaximizable() const
{
    if (!isResizable() || isToolbar())  // SELI isToolbar() ?
        return false;
    if (rules()->checkMaximize(MaximizeRestore) == MaximizeRestore && rules()->checkMaximize(MaximizeFull) != MaximizeRestore)
        return true;
    return false;
}


/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::setGeometry(int x, int y, int w, int h, ForceGeometry_t force)
{
    // this code is also duplicated in Client::plainResize()
    // Ok, the shading geometry stuff. Generally, code doesn't care about shaded geometry,
    // simply because there are too many places dealing with geometry. Those places
    // ignore shaded state and use normal geometry, which they usually should get
    // from adjustedSize(). Such geometry comes here, and if the window is shaded,
    // the geometry is used only for client_size, since that one is not used when
    // shading. Then the frame geometry is adjusted for the shaded geometry.
    // This gets more complicated in the case the code does only something like
    // setGeometry( geometry()) - geometry() will return the shaded frame geometry.
    // Such code is wrong and should be changed to handle the case when the window is shaded,
    // for example using Client::clientSize()

    if (shade_geometry_change)
        ; // nothing
    else if (isShade()) {
        if (h == borderTop() + borderBottom()) {
            qCDebug(KWIN_CORE) << "Shaded geometry passed for size:";
        } else {
            client_size = QSize(w - borderLeft() - borderRight(), h - borderTop() - borderBottom());
            h = borderTop() + borderBottom();
        }
    } else {
        client_size = QSize(w - borderLeft() - borderRight(), h - borderTop() - borderBottom());
    }
    QRect g(x, y, w, h);
    if (!areGeometryUpdatesBlocked() && g != rules()->checkGeometry(g)) {
        qCDebug(KWIN_CORE) << "forced geometry fail:" << g << ":" << rules()->checkGeometry(g);
    }
    if (force == NormalGeometrySet && geom == g && pendingGeometryUpdate() == PendingGeometryNone)
        return;
    geom = g;
    if (areGeometryUpdatesBlocked()) {
        if (pendingGeometryUpdate() == PendingGeometryForced)
            {} // maximum, nothing needed
        else if (force == ForceGeometrySet)
            setPendingGeometryUpdate(PendingGeometryForced);
        else
            setPendingGeometryUpdate(PendingGeometryNormal);
        return;
    }
    QSize oldClientSize = m_frame.geometry().size();
    bool resized = (geometryBeforeUpdateBlocking().size() != geom.size() || pendingGeometryUpdate() == PendingGeometryForced);
    if (resized) {
        resizeDecoration();
        m_frame.setGeometry(x, y, w, h);
        if (!isShade()) {
            QSize cs = clientSize();
            m_wrapper.setGeometry(QRect(clientPos(), cs));
            if (!isResize() || syncRequest.counter == XCB_NONE)
                m_client.setGeometry(0, 0, cs.width(), cs.height());
            // SELI - won't this be too expensive?
            // THOMAS - yes, but gtk+ clients will not resize without ...
            sendSyntheticConfigureNotify();
        }
        updateShape();
    } else {
        if (isMoveResize()) {
            if (compositing())  // Defer the X update until we leave this mode
                needsXWindowMove = true;
            else
                m_frame.move(x, y); // sendSyntheticConfigureNotify() on finish shall be sufficient
        } else {
            m_frame.move(x, y);
            sendSyntheticConfigureNotify();
        }

        // Unconditionally move the input window: it won't affect rendering
        m_decoInputExtent.move(QPoint(x, y) + inputPos());
    }
    updateWindowRules(Rules::Position|Rules::Size);

    // keep track of old maximize mode
    // to detect changes
    screens()->setCurrent(this);
    workspace()->updateStackingOrder();

    // need to regenerate decoration pixmaps when either
    // - size is changed
    // - maximize mode is changed to MaximizeRestore, when size unchanged
    //   which can happen when untabbing maximized windows
    if (resized) {
        if (oldClientSize != QSize(w,h))
            discardWindowPixmap();
    }
    emit geometryShapeChanged(this, geometryBeforeUpdateBlocking());
    addRepaintDuringGeometryUpdates();
    updateGeometryBeforeUpdateBlocking();

    // Update states of all other windows in this group
    if (tabGroup())
        tabGroup()->updateStates(this, TabGroup::Geometry);

    // TODO: this signal is emitted too often
    emit geometryChanged();
}

void Client::plainResize(int w, int h, ForceGeometry_t force)
{
    // this code is also duplicated in Client::setGeometry(), and it's also commented there
    if (shade_geometry_change)
        ; // nothing
    else if (isShade()) {
        if (h == borderTop() + borderBottom()) {
            qCDebug(KWIN_CORE) << "Shaded geometry passed for size:";
        } else {
            client_size = QSize(w - borderLeft() - borderRight(), h - borderTop() - borderBottom());
            h = borderTop() + borderBottom();
        }
    } else {
        client_size = QSize(w - borderLeft() - borderRight(), h - borderTop() - borderBottom());
    }
    QSize s(w, h);
    if (!areGeometryUpdatesBlocked() && s != rules()->checkSize(s)) {
        qCDebug(KWIN_CORE) << "forced size fail:" << s << ":" << rules()->checkSize(s);
    }
    // resuming geometry updates is handled only in setGeometry()
    assert(pendingGeometryUpdate() == PendingGeometryNone || areGeometryUpdatesBlocked());
    if (force == NormalGeometrySet && geom.size() == s)
        return;
    geom.setSize(s);
    if (areGeometryUpdatesBlocked()) {
        if (pendingGeometryUpdate() == PendingGeometryForced)
            {} // maximum, nothing needed
        else if (force == ForceGeometrySet)
            setPendingGeometryUpdate(PendingGeometryForced);
        else
            setPendingGeometryUpdate(PendingGeometryNormal);
        return;
    }
    QSize oldClientSize = m_frame.geometry().size();
    resizeDecoration();
    m_frame.resize(w, h);
//     resizeDecoration( s );
    if (!isShade()) {
        QSize cs = clientSize();
        m_wrapper.setGeometry(QRect(clientPos(), cs));
        m_client.setGeometry(0, 0, cs.width(), cs.height());
    }
    updateShape();

    sendSyntheticConfigureNotify();
    updateWindowRules(Rules::Position|Rules::Size);
    screens()->setCurrent(this);
    workspace()->updateStackingOrder();
    if (oldClientSize != QSize(w,h))
        discardWindowPixmap();
    emit geometryShapeChanged(this, geometryBeforeUpdateBlocking());
    addRepaintDuringGeometryUpdates();
    updateGeometryBeforeUpdateBlocking();

    // Update states of all other windows in this group
    if (tabGroup())
        tabGroup()->updateStates(this, TabGroup::Geometry);
    // TODO: this signal is emitted too often
    emit geometryChanged();
}

/*!
  Reimplemented to inform the client about the new window position.
 */
void AbstractClient::move(int x, int y, ForceGeometry_t force)
{
    // resuming geometry updates is handled only in setGeometry()
    assert(pendingGeometryUpdate() == PendingGeometryNone || areGeometryUpdatesBlocked());
    QPoint p(x, y);
    if (!areGeometryUpdatesBlocked() && p != rules()->checkPosition(p)) {
        qCDebug(KWIN_CORE) << "forced position fail:" << p << ":" << rules()->checkPosition(p);
    }
    if (force == NormalGeometrySet && geom.topLeft() == p)
        return;
    geom.moveTopLeft(p);
    if (areGeometryUpdatesBlocked()) {
        if (pendingGeometryUpdate() == PendingGeometryForced)
            {} // maximum, nothing needed
        else if (force == ForceGeometrySet)
            setPendingGeometryUpdate(PendingGeometryForced);
        else
            setPendingGeometryUpdate(PendingGeometryNormal);
        return;
    }
    doMove(x, y);
    updateWindowRules(Rules::Position);
    screens()->setCurrent(this);
    workspace()->updateStackingOrder();
    // client itself is not damaged
    addRepaintDuringGeometryUpdates();
    updateGeometryBeforeUpdateBlocking();

    // Update states of all other windows in this group
    updateTabGroupStates(TabGroup::Geometry);
    emit geometryChanged();
}

void Client::doMove(int x, int y)
{
    m_frame.move(x, y);
    sendSyntheticConfigureNotify();
}

void AbstractClient::blockGeometryUpdates(bool block)
{
    if (block) {
        if (m_blockGeometryUpdates == 0)
            m_pendingGeometryUpdate = PendingGeometryNone;
        ++m_blockGeometryUpdates;
    } else {
        if (--m_blockGeometryUpdates == 0) {
            if (m_pendingGeometryUpdate != PendingGeometryNone) {
                if (isShade())
                    setGeometry(QRect(pos(), adjustedSize()), NormalGeometrySet);
                else
                    setGeometry(geometry(), NormalGeometrySet);
                m_pendingGeometryUpdate = PendingGeometryNone;
            }
        }
    }
}

void AbstractClient::maximize(MaximizeMode m)
{
    if (m == maximizeMode()) {
        return;
    }
    setMaximize(m & MaximizeVertical, m & MaximizeHorizontal);
}

/*!
  Sets the maximization according to \a vertically and \a horizontally
 */
void AbstractClient::setMaximize(bool vertically, bool horizontally)
{
    // changeMaximize() flips the state, so change from set->flip
    const MaximizeMode oldMode = maximizeMode();
    changeMaximize(
        oldMode & MaximizeVertical ? !vertically : vertically,
        oldMode & MaximizeHorizontal ? !horizontally : horizontally,
        false);
    const MaximizeMode newMode = maximizeMode();
    if (oldMode != newMode) {
        emit clientMaximizedStateChanged(this, newMode);
        emit clientMaximizedStateChanged(this, vertically, horizontally);
    }

}

// Update states of all other windows in this group
class TabSynchronizer
{
public:
    TabSynchronizer(AbstractClient *client, TabGroup::States syncStates) :
    m_client(client) , m_states(syncStates)
    {
        if (client->tabGroup())
            client->tabGroup()->blockStateUpdates(true);
    }
    ~TabSynchronizer()
    {
        syncNow();
    }
    void syncNow()
    {
        if (m_client && m_client->tabGroup()) {
            m_client->tabGroup()->blockStateUpdates(false);
            m_client->tabGroup()->updateStates(dynamic_cast<Client*>(m_client), m_states);
        }
        m_client = 0;
    }
private:
    AbstractClient *m_client;
    TabGroup::States m_states;
};


static bool changeMaximizeRecursion = false;
void Client::changeMaximize(bool vertical, bool horizontal, bool adjust)
{
    if (changeMaximizeRecursion)
        return;

    if (!isResizable() || isToolbar())  // SELI isToolbar() ?
        return;

    QRect clientArea;
    if (isElectricBorderMaximizing())
        clientArea = workspace()->clientArea(MaximizeArea, Cursor::pos(), desktop());
    else
        clientArea = workspace()->clientArea(MaximizeArea, this);

    MaximizeMode old_mode = max_mode;
    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if (!adjust) {
        if (vertical)
            max_mode = MaximizeMode(max_mode ^ MaximizeVertical);
        if (horizontal)
            max_mode = MaximizeMode(max_mode ^ MaximizeHorizontal);
    }

    // if the client insist on a fix aspect ratio, we check whether the maximizing will get us
    // out of screen bounds and take that as a "full maximization with aspect check" then
    if (m_geometryHints.hasAspect() && // fixed aspect
        (max_mode == MaximizeVertical || max_mode == MaximizeHorizontal) && // ondimensional maximization
        rules()->checkStrictGeometry(true)) { // obey aspect
        const QSize minAspect = m_geometryHints.minAspect();
        const QSize maxAspect = m_geometryHints.maxAspect();
        if (max_mode == MaximizeVertical || (old_mode & MaximizeVertical)) {
            const double fx = minAspect.width(); // use doubles, because the values can be MAX_INT
            const double fy = maxAspect.height(); // use doubles, because the values can be MAX_INT
            if (fx*clientArea.height()/fy > clientArea.width()) // too big
                max_mode = old_mode & MaximizeHorizontal ? MaximizeRestore : MaximizeFull;
        } else { // max_mode == MaximizeHorizontal
            const double fx = maxAspect.width();
            const double fy = minAspect.height();
            if (fy*clientArea.width()/fx > clientArea.height()) // too big
                max_mode = old_mode & MaximizeVertical ? MaximizeRestore : MaximizeFull;
        }
    }

    max_mode = rules()->checkMaximize(max_mode);
    if (!adjust && max_mode == old_mode)
        return;

    GeometryUpdatesBlocker blocker(this);
    // QT synchronizing required because we eventually change from QT to Maximized
    TabSynchronizer syncer(this, TabGroup::Maximized|TabGroup::QuickTile);

    // maximing one way and unmaximizing the other way shouldn't happen,
    // so restore first and then maximize the other way
    if ((old_mode == MaximizeVertical && max_mode == MaximizeHorizontal)
            || (old_mode == MaximizeHorizontal && max_mode == MaximizeVertical)) {
        changeMaximize(false, false, false);   // restore
    }

    // save sizes for restoring, if maximalizing
    QSize sz;
    if (isShade())
        sz = sizeForClientSize(clientSize());
    else
        sz = size();

    if (quickTileMode() == QuickTileMode(QuickTileFlag::None)) {
        if (!adjust && !(old_mode & MaximizeVertical)) {
            geom_restore.setTop(y());
            geom_restore.setHeight(sz.height());
        }
        if (!adjust && !(old_mode & MaximizeHorizontal)) {
            geom_restore.setLeft(x());
            geom_restore.setWidth(sz.width());
        }
    }

    // call into decoration update borders
    if (isDecorated() && decoration()->client() && !(options->borderlessMaximizedWindows() && max_mode == KWin::MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = decoration()->client().data();
        if ((max_mode & MaximizeVertical) != (old_mode & MaximizeVertical)) {
            emit c->maximizedVerticallyChanged(max_mode & MaximizeVertical);
        }
        if ((max_mode & MaximizeHorizontal) != (old_mode & MaximizeHorizontal)) {
            emit c->maximizedHorizontallyChanged(max_mode & MaximizeHorizontal);
        }
        if ((max_mode == MaximizeFull) != (old_mode == MaximizeFull)) {
            emit c->maximizedChanged(max_mode & MaximizeFull);
        }
        changeMaximizeRecursion = false;
    }

    if (options->borderlessMaximizedWindows()) {
        // triggers a maximize change.
        // The next setNoBorder interation will exit since there's no change but the first recursion pullutes the restore geometry
        changeMaximizeRecursion = true;
        setNoBorder(rules()->checkNoBorder(app_noborder || (m_motif.hasDecoration() && m_motif.noBorder()) || max_mode == MaximizeFull));
        changeMaximizeRecursion = false;
    }

    const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;

    // Conditional quick tiling exit points
    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        if (old_mode == MaximizeFull &&
                !clientArea.contains(geom_restore.center())) {
            // Not restoring on the same screen
            // TODO: The following doesn't work for some reason
            //quick_tile_mode = QuickTileFlag::None; // And exit quick tile mode manually
        } else if ((old_mode == MaximizeVertical && max_mode == MaximizeRestore) ||
                  (old_mode == MaximizeFull && max_mode == MaximizeHorizontal)) {
            // Modifying geometry of a tiled window
            updateQuickTileMode(QuickTileFlag::None); // Exit quick tile mode without restoring geometry
        }
    }

    switch(max_mode) {

    case MaximizeVertical: {
        if (old_mode & MaximizeHorizontal) { // actually restoring from MaximizeFull
            if (geom_restore.width() == 0 || !clientArea.contains(geom_restore.center())) {
                // needs placement
                plainResize(adjustedSize(QSize(width() * 2 / 3, clientArea.height()), SizemodeFixedH), geom_mode);
                Placement::self()->placeSmart(this, clientArea);
            } else {
                setGeometry(QRect(QPoint(geom_restore.x(), clientArea.top()),
                                  adjustedSize(QSize(geom_restore.width(), clientArea.height()), SizemodeFixedH)), geom_mode);
            }
        } else {
            QRect r(x(), clientArea.top(), width(), clientArea.height());
            r.setTopLeft(rules()->checkPosition(r.topLeft()));
            r.setSize(adjustedSize(r.size(), SizemodeFixedH));
            setGeometry(r, geom_mode);
        }
        info->setState(NET::MaxVert, NET::Max);
        break;
    }

    case MaximizeHorizontal: {
        if (old_mode & MaximizeVertical) { // actually restoring from MaximizeFull
            if (geom_restore.height() == 0 || !clientArea.contains(geom_restore.center())) {
                // needs placement
                plainResize(adjustedSize(QSize(clientArea.width(), height() * 2 / 3), SizemodeFixedW), geom_mode);
                Placement::self()->placeSmart(this, clientArea);
            } else {
                setGeometry(QRect(QPoint(clientArea.left(), geom_restore.y()),
                                  adjustedSize(QSize(clientArea.width(), geom_restore.height()), SizemodeFixedW)), geom_mode);
            }
        } else {
            QRect r(clientArea.left(), y(), clientArea.width(), height());
            r.setTopLeft(rules()->checkPosition(r.topLeft()));
            r.setSize(adjustedSize(r.size(), SizemodeFixedW));
            setGeometry(r, geom_mode);
        }
        info->setState(NET::MaxHoriz, NET::Max);
        break;
    }

    case MaximizeRestore: {
        QRect restore = geometry();
        // when only partially maximized, geom_restore may not have the other dimension remembered
        if (old_mode & MaximizeVertical) {
            restore.setTop(geom_restore.top());
            restore.setBottom(geom_restore.bottom());
        }
        if (old_mode & MaximizeHorizontal) {
            restore.setLeft(geom_restore.left());
            restore.setRight(geom_restore.right());
        }
        if (!restore.isValid()) {
            QSize s = QSize(clientArea.width() * 2 / 3, clientArea.height() * 2 / 3);
            if (geom_restore.width() > 0)
                s.setWidth(geom_restore.width());
            if (geom_restore.height() > 0)
                s.setHeight(geom_restore.height());
            plainResize(adjustedSize(s));
            Placement::self()->placeSmart(this, clientArea);
            restore = geometry();
            if (geom_restore.width() > 0)
                restore.moveLeft(geom_restore.x());
            if (geom_restore.height() > 0)
                restore.moveTop(geom_restore.y());
            geom_restore = restore; // relevant for mouse pos calculation, bug #298646
        }
        if (m_geometryHints.hasAspect()) {
            restore.setSize(adjustedSize(restore.size(), SizemodeAny));
        }
        setGeometry(restore, geom_mode);
        if (!clientArea.contains(geom_restore.center()))    // Not restoring to the same screen
            Placement::self()->place(this, clientArea);
        info->setState(0, NET::Max);
        updateQuickTileMode(QuickTileFlag::None);
        break;
    }

    case MaximizeFull: {
        QRect r(clientArea);
        r.setTopLeft(rules()->checkPosition(r.topLeft()));
        r.setSize(adjustedSize(r.size(), SizemodeMax));
        if (r.size() != clientArea.size()) { // to avoid off-by-one errors...
            if (isElectricBorderMaximizing() && r.width() < clientArea.width()) {
                r.moveLeft(qMax(clientArea.left(), Cursor::pos().x() - r.width()/2));
                r.moveRight(qMin(clientArea.right(), r.right()));
            } else {
                r.moveCenter(clientArea.center());
                const bool closeHeight = r.height() > 97*clientArea.height()/100;
                const bool closeWidth  = r.width()  > 97*clientArea.width() /100;
                const bool overHeight = r.height() > clientArea.height();
                const bool overWidth  = r.width()  > clientArea.width();
                if (closeWidth || closeHeight) {
                    Position titlePos = titlebarPosition();
                    const QRect screenArea = workspace()->clientArea(ScreenArea, clientArea.center(), desktop());
                    if (closeHeight) {
                        bool tryBottom = titlePos == PositionBottom;
                        if ((overHeight && titlePos == PositionTop) ||
                            screenArea.top() == clientArea.top())
                            r.setTop(clientArea.top());
                        else
                            tryBottom = true;
                        if (tryBottom &&
                            (overHeight || screenArea.bottom() == clientArea.bottom()))
                            r.setBottom(clientArea.bottom());
                    }
                    if (closeWidth) {
                        bool tryLeft = titlePos == PositionLeft;
                        if ((overWidth && titlePos == PositionRight) ||
                            screenArea.right() == clientArea.right())
                            r.setRight(clientArea.right());
                        else
                            tryLeft = true;
                        if (tryLeft && (overWidth || screenArea.left() == clientArea.left()))
                            r.setLeft(clientArea.left());
                    }
                }
            }
            r.moveTopLeft(rules()->checkPosition(r.topLeft()));
        }
        setGeometry(r, geom_mode);
        if (options->electricBorderMaximize() && r.top() == clientArea.top())
            updateQuickTileMode(QuickTileFlag::Maximize);
        else
            updateQuickTileMode(QuickTileFlag::None);
        info->setState(NET::Max, NET::Max);
        break;
    }
    default:
        break;
    }

    syncer.syncNow(); // important because of window rule updates!

    updateAllowedActions();
    updateWindowRules(Rules::MaximizeVert|Rules::MaximizeHoriz|Rules::Position|Rules::Size);
    emit quickTileModeChanged();
}

bool AbstractClient::isFullScreenable() const
{
    return isFullScreenable(false);
}

bool AbstractClient::isFullScreenable(bool fullscreen_hack) const
{
    if (!rules()->checkFullScreen(true))
        return false;
    if (fullscreen_hack)
        return isNormalWindow();
    if (rules()->checkStrictGeometry(true)) { // allow rule to ignore geometry constraints
        QRect fsarea = workspace()->clientArea(FullScreenArea, this);
        if (sizeForClientSize(fsarea.size(), SizemodeAny, true) != fsarea.size())
            return false; // the app wouldn't fit exactly fullscreen geometry due to its strict geometry requirements
    }
    // don't check size constrains - some apps request fullscreen despite requesting fixed size
    return !isSpecialWindow(); // also better disallow only weird types to go fullscreen
}

bool Client::userCanSetFullScreen() const
{
    if (fullscreen_mode == FullScreenHack)
        return false;
    if (!isFullScreenable(false))
        return false;
    return isNormalWindow() || isDialog();
}

void Client::setFullScreen(bool set, bool user)
{
    if (!isFullScreen() && !set)
        return;
    if (fullscreen_mode == FullScreenHack)
        return;
    if (user && !userCanSetFullScreen())
        return;
    set = rules()->checkFullScreen(set && !isSpecialWindow());
    setShade(ShadeNone);
    bool was_fs = isFullScreen();
    if (was_fs)
        workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event
    else
        geom_fs_restore = geometry();
    fullscreen_mode = set ? FullScreenNormal : FullScreenNone;
    if (was_fs == isFullScreen())
        return;
    if (set) {
        untab();
        workspace()->raiseClient(this);
    }
    StackingUpdatesBlocker blocker1(workspace());
    GeometryUpdatesBlocker blocker2(this);
    workspace()->updateClientLayer(this);   // active fullscreens get different layer
    info->setState(isFullScreen() ? NET::FullScreen : NET::States(0), NET::FullScreen);
    updateDecoration(false, false);
    if (isFullScreen()) {
        if (info->fullscreenMonitors().isSet())
            setGeometry(fullscreenMonitorsArea(info->fullscreenMonitors()));
        else
            setGeometry(workspace()->clientArea(FullScreenArea, this));
    }
    else {
        if (!geom_fs_restore.isNull()) {
            int currentScreen = screen();
            setGeometry(QRect(geom_fs_restore.topLeft(), adjustedSize(geom_fs_restore.size())));
            if( currentScreen != screen())
                workspace()->sendClientToScreen( this, currentScreen );
        // TODO isShaded() ?
        } else {
            // does this ever happen?
            setGeometry(workspace()->clientArea(MaximizeArea, this));
        }
    }
    updateWindowRules(Rules::Fullscreen|Rules::Position|Rules::Size);

    if (was_fs != isFullScreen()) {
        emit clientFullScreenSet(this, set, user);
        emit fullScreenChanged();
    }
}


void Client::updateFullscreenMonitors(NETFullscreenMonitors topology)
{
    int nscreens = screens()->count();

//    qDebug() << "incoming request with top: " << topology.top << " bottom: " << topology.bottom
//                   << " left: " << topology.left << " right: " << topology.right
//                   << ", we have: " << nscreens << " screens.";

    if (topology.top >= nscreens ||
            topology.bottom >= nscreens ||
            topology.left >= nscreens ||
            topology.right >= nscreens) {
        qCWarning(KWIN_CORE) << "fullscreenMonitors update failed. request higher than number of screens.";
        return;
    }

    info->setFullscreenMonitors(topology);
    if (isFullScreen())
        setGeometry(fullscreenMonitorsArea(topology));
}


/*!
  Calculates the bounding rectangle defined by the 4 monitor indices indicating the
  top, bottom, left, and right edges of the window when the fullscreen state is enabled.
 */
QRect Client::fullscreenMonitorsArea(NETFullscreenMonitors requestedTopology) const
{
    QRect top, bottom, left, right, total;

    top = screens()->geometry(requestedTopology.top);
    bottom = screens()->geometry(requestedTopology.bottom);
    left = screens()->geometry(requestedTopology.left);
    right = screens()->geometry(requestedTopology.right);
    total = top.united(bottom.united(left.united(right)));

//    qDebug() << "top: " << top << " bottom: " << bottom
//                   << " left: " << left << " right: " << right;
//    qDebug() << "returning rect: " << total;
    return total;
}


int Client::checkFullScreenHack(const QRect& geom) const
{
    if (!options->isLegacyFullscreenSupport())
        return 0;
    // if it's noborder window, and has size of one screen or the whole desktop geometry, it's fullscreen hack
    if (noBorder() && app_noborder && isFullScreenable(true)) {
        if (geom.size() == workspace()->clientArea(FullArea, geom.center(), desktop()).size())
            return 2; // full area fullscreen hack
        if (geom.size() == workspace()->clientArea(ScreenArea, geom.center(), desktop()).size())
            return 1; // xinerama-aware fullscreen hack
    }
    return 0;
}

void Client::updateFullScreenHack(const QRect& geom)
{
    int type = checkFullScreenHack(geom);
    if (fullscreen_mode == FullScreenNone && type != 0) {
        fullscreen_mode = FullScreenHack;
        updateDecoration(false, false);
        QRect geom;
        if (rules()->checkStrictGeometry(false)) {
            geom = type == 2 // 1 - it's xinerama-aware fullscreen hack, 2 - it's full area
                   ? workspace()->clientArea(FullArea, geom.center(), desktop())
                   : workspace()->clientArea(ScreenArea, geom.center(), desktop());
        } else
            geom = workspace()->clientArea(FullScreenArea, geom.center(), desktop());
        setGeometry(geom);
        emit fullScreenChanged();
    } else if (fullscreen_mode == FullScreenHack && type == 0) {
        fullscreen_mode = FullScreenNone;
        updateDecoration(false, false);
        // whoever called this must setup correct geometry
        emit fullScreenChanged();
    }
    StackingUpdatesBlocker blocker(workspace());
    workspace()->updateClientLayer(this);   // active fullscreens get different layer
}

static GeometryTip* geometryTip    = 0;

void Client::positionGeometryTip()
{
    assert(isMove() || isResize());
    // Position and Size display
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::GeometryTip))
        return; // some effect paints this for us
    if (options->showGeometryTip()) {
        if (!geometryTip) {
            geometryTip = new GeometryTip(&m_geometryHints);
        }
        QRect wgeom(moveResizeGeometry());   // position of the frame, size of the window itself
        wgeom.setWidth(wgeom.width() - (width() - clientSize().width()));
        wgeom.setHeight(wgeom.height() - (height() - clientSize().height()));
        if (isShade())
            wgeom.setHeight(0);
        geometryTip->setGeometry(wgeom);
        if (!geometryTip->isVisible())
            geometryTip->show();
        geometryTip->raise();
    }
}

bool AbstractClient::startMoveResize()
{
    assert(!isMoveResize());
    assert(QWidget::keyboardGrabber() == NULL);
    assert(QWidget::mouseGrabber() == NULL);
    stopDelayedMoveResize();
    if (QApplication::activePopupWidget() != NULL)
        return false; // popups have grab
    if (isFullScreen() && (screens()->count() < 2 || !isMovableAcrossScreens()))
        return false;
    if (!doStartMoveResize()) {
        return false;
    }

    invalidateDecorationDoubleClickTimer();

    setMoveResize(true);
    workspace()->setClientIsMoving(this);

    const Position mode = moveResizePointerMode();
    if (mode != PositionCenter) { // means "isResize()" but moveResizeMode = true is set below
        if (maximizeMode() == MaximizeFull) { // partial is cond. reset in finishMoveResize
            setGeometryRestore(geometry()); // "restore" to current geometry
            setMaximize(false, false);
        }
    }

    if (quickTileMode() != QuickTileMode(QuickTileFlag::None) && mode != PositionCenter) { // Cannot use isResize() yet
        // Exit quick tile mode when the user attempts to resize a tiled window
        updateQuickTileMode(QuickTileFlag::None); // Do so without restoring original geometry
        setGeometryRestore(geometry());
        emit quickTileModeChanged();
    }

    updateHaveResizeEffect();
    updateInitialMoveResizeGeometry();
    checkUnrestrictedMoveResize();
    emit clientStartUserMovedResized(this);
    if (ScreenEdges::self()->isDesktopSwitchingMovingClients())
        ScreenEdges::self()->reserveDesktopSwitching(true, Qt::Vertical|Qt::Horizontal);
    return true;
}

bool Client::doStartMoveResize()
{
    bool has_grab = false;
    // This reportedly improves smoothness of the moveresize operation,
    // something with Enter/LeaveNotify events, looks like XFree performance problem or something *shrug*
    // (http://lists.kde.org/?t=107302193400001&r=1&w=2)
    QRect r = workspace()->clientArea(FullArea, this);
    m_moveResizeGrabWindow.create(r, XCB_WINDOW_CLASS_INPUT_ONLY, 0, NULL, rootWindow());
    m_moveResizeGrabWindow.map();
    m_moveResizeGrabWindow.raise();
    updateXTime();
    const xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer_unchecked(connection(), false, m_moveResizeGrabWindow,
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, m_moveResizeGrabWindow, Cursor::x11Cursor(cursor()), xTime());
    ScopedCPointer<xcb_grab_pointer_reply_t> pointerGrab(xcb_grab_pointer_reply(connection(), cookie, NULL));
    if (!pointerGrab.isNull() && pointerGrab->status == XCB_GRAB_STATUS_SUCCESS) {
        has_grab = true;
    }
    if (!has_grab && grabXKeyboard(frameId()))
        has_grab = move_resize_has_keyboard_grab = true;
    if (!has_grab) { // at least one grab is necessary in order to be able to finish move/resize
        m_moveResizeGrabWindow.reset();
        return false;
    }
    return true;
}

void AbstractClient::finishMoveResize(bool cancel)
{
    const bool wasResize = isResize(); // store across leaveMoveResize
    leaveMoveResize();

    if (cancel)
        setGeometry(initialMoveResizeGeometry());
    else {
        const QRect &moveResizeGeom = moveResizeGeometry();
        if (wasResize) {
            const bool restoreH = maximizeMode() == MaximizeHorizontal &&
                                    moveResizeGeom.width() != initialMoveResizeGeometry().width();
            const bool restoreV = maximizeMode() == MaximizeVertical &&
                                    moveResizeGeom.height() != initialMoveResizeGeometry().height();
            if (restoreH || restoreV) {
                changeMaximize(restoreV, restoreH, false);
            }
        }
        setGeometry(moveResizeGeom);
    }
    checkScreen(); // needs to be done because clientFinishUserMovedResized has not yet re-activated online alignment
    if (screen() != moveResizeStartScreen()) {
        workspace()->sendClientToScreen(this, screen()); // checks rule validity
        if (maximizeMode() != MaximizeRestore)
            checkWorkspacePosition();
    }

    if (isElectricBorderMaximizing()) {
        setQuickTileMode(electricBorderMode());
        setElectricBorderMaximizing(false);
    } else if (!cancel) {
        QRect geom_restore = geometryRestore();
        if (!(maximizeMode() & MaximizeHorizontal)) {
            geom_restore.setX(geometry().x());
            geom_restore.setWidth(geometry().width());
        }
        if (!(maximizeMode() & MaximizeVertical)) {
            geom_restore.setY(geometry().y());
            geom_restore.setHeight(geometry().height());
        }
        setGeometryRestore(geom_restore);
    }
// FRAME    update();

    emit clientFinishUserMovedResized(this);
}

void Client::leaveMoveResize()
{
    if (needsXWindowMove) {
        // Do the deferred move
        m_frame.move(geom.topLeft());
        needsXWindowMove = false;
    }
    if (!isResize())
        sendSyntheticConfigureNotify(); // tell the client about it's new final position
    if (geometryTip) {
        geometryTip->hide();
        delete geometryTip;
        geometryTip = NULL;
    }
    if (move_resize_has_keyboard_grab)
        ungrabXKeyboard();
    move_resize_has_keyboard_grab = false;
    xcb_ungrab_pointer(connection(), xTime());
    m_moveResizeGrabWindow.reset();
    if (syncRequest.counter == XCB_NONE) // don't forget to sanitize since the timeout will no more fire
        syncRequest.isPending = false;
    delete syncRequest.timeout;
    syncRequest.timeout = NULL;
    AbstractClient::leaveMoveResize();
}

// This function checks if it actually makes sense to perform a restricted move/resize.
// If e.g. the titlebar is already outside of the workarea, there's no point in performing
// a restricted move resize, because then e.g. resize would also move the window (#74555).
// NOTE: Most of it is duplicated from handleMoveResize().
void AbstractClient::checkUnrestrictedMoveResize()
{
    if (isUnrestrictedMoveResize())
        return;
    const QRect &moveResizeGeom = moveResizeGeometry();
    QRect desktopArea = workspace()->clientArea(WorkArea, moveResizeGeom.center(), desktop());
    int left_marge, right_marge, top_marge, bottom_marge, titlebar_marge;
    // restricted move/resize - keep at least part of the titlebar always visible
    // how much must remain visible when moved away in that direction
    left_marge = qMin(100 + borderRight(), moveResizeGeom.width());
    right_marge = qMin(100 + borderLeft(), moveResizeGeom.width());
    // width/height change with opaque resizing, use the initial ones
    titlebar_marge = initialMoveResizeGeometry().height();
    top_marge = borderBottom();
    bottom_marge = borderTop();
    if (isResize()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + top_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.right() < desktopArea.left() + left_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.left() > desktopArea.right() - right_marge)
            setUnrestrictedMoveResize(true);
        if (!isUnrestrictedMoveResize() && moveResizeGeom.top() < desktopArea.top())   // titlebar mustn't go out
            setUnrestrictedMoveResize(true);
    }
    if (isMove()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + titlebar_marge - 1)
            setUnrestrictedMoveResize(true);
        // no need to check top_marge, titlebar_marge already handles it
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge + 1) // titlebar mustn't go out
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.right() < desktopArea.left() + left_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.left() > desktopArea.right() - right_marge)
            setUnrestrictedMoveResize(true);
    }
}

// When the user pressed mouse on the titlebar, don't activate move immediatelly,
// since it may be just a click. Activate instead after a delay. Move used to be
// activated only after moving by several pixels, but that looks bad.
void AbstractClient::startDelayedMoveResize()
{
    Q_ASSERT(!m_moveResize.delayedTimer);
    m_moveResize.delayedTimer = new QTimer(this);
    m_moveResize.delayedTimer->setSingleShot(true);
    connect(m_moveResize.delayedTimer, &QTimer::timeout, this,
        [this]() {
            assert(isMoveResizePointerButtonDown());
            if (!startMoveResize()) {
                setMoveResizePointerButtonDown(false);
            }
            updateCursor();
            stopDelayedMoveResize();
        }
    );
    m_moveResize.delayedTimer->start(QApplication::startDragTime());
}

void AbstractClient::stopDelayedMoveResize()
{
    delete m_moveResize.delayedTimer;
    m_moveResize.delayedTimer = nullptr;
}

void AbstractClient::handleMoveResize(const QPoint &local, const QPoint &global)
{
    const QRect oldGeo = geometry();
    handleMoveResize(local.x(), local.y(), global.x(), global.y());
    if (!isFullScreen() && isMove()) {
        if (quickTileMode() != QuickTileMode(QuickTileFlag::None) && oldGeo != geometry()) {
            GeometryUpdatesBlocker blocker(this);
            setQuickTileMode(QuickTileFlag::None);
            const QRect &geom_restore = geometryRestore();
            setMoveOffset(QPoint(double(moveOffset().x()) / double(oldGeo.width()) * double(geom_restore.width()),
                                 double(moveOffset().y()) / double(oldGeo.height()) * double(geom_restore.height())));
            if (rules()->checkMaximize(MaximizeRestore) == MaximizeRestore)
                setMoveResizeGeometry(geom_restore);
            handleMoveResize(local.x(), local.y(), global.x(), global.y()); // fix position
        } else if (quickTileMode() == QuickTileMode(QuickTileFlag::None) && isResizable()) {
            checkQuickTilingMaximizationZones(global.x(), global.y());
        }
    }
}

bool Client::isWaitingForMoveResizeSync() const
{
    return syncRequest.isPending && isResize();
}

void AbstractClient::handleMoveResize(int x, int y, int x_root, int y_root)
{
    if (isWaitingForMoveResizeSync())
        return; // we're still waiting for the client or the timeout

    const Position mode = moveResizePointerMode();
    if ((mode == PositionCenter && !isMovableAcrossScreens())
            || (mode != PositionCenter && (isShade() || !isResizable())))
        return;

    if (!isMoveResize()) {
        QPoint p(QPoint(x/* - padding_left*/, y/* - padding_top*/) - moveOffset());
        if (p.manhattanLength() >= QApplication::startDragDistance()) {
            if (!startMoveResize()) {
                setMoveResizePointerButtonDown(false);
                updateCursor();
                return;
            }
            updateCursor();
        } else
            return;
    }

    // ShadeHover or ShadeActive, ShadeNormal was already avoided above
    if (mode != PositionCenter && shadeMode() != ShadeNone)
        setShade(ShadeNone);

    QPoint globalPos(x_root, y_root);
    // these two points limit the geometry rectangle, i.e. if bottomleft resizing is done,
    // the bottomleft corner should be at is at (topleft.x(), bottomright().y())
    QPoint topleft = globalPos - moveOffset();
    QPoint bottomright = globalPos + invertedMoveOffset();
    QRect previousMoveResizeGeom = moveResizeGeometry();

    // TODO move whole group when moving its leader or when the leader is not mapped?

    auto titleBarRect = [this](bool &transposed, int &requiredPixels) -> QRect {
        const QRect &moveResizeGeom = moveResizeGeometry();
        QRect r(moveResizeGeom);
        r.moveTopLeft(QPoint(0,0));
        switch (titlebarPosition()) {
        default:
        case PositionTop:
            r.setHeight(borderTop());
            break;
        case PositionLeft:
            r.setWidth(borderLeft());
            transposed = true;
            break;
        case PositionBottom:
            r.setTop(r.bottom() - borderBottom());
            break;
        case PositionRight:
            r.setLeft(r.right() - borderRight());
            transposed = true;
            break;
        }
        // When doing a restricted move we must always keep 100px of the titlebar
        // visible to allow the user to be able to move it again.
        requiredPixels = qMin(100 * (transposed ? r.width() : r.height()),
                              moveResizeGeom.width() * moveResizeGeom.height());
        return r;
    };

    bool update = false;
    if (isResize()) {
        QRect orig = initialMoveResizeGeometry();
        Sizemode sizemode = SizemodeAny;
        auto calculateMoveResizeGeom = [this, &topleft, &bottomright, &orig, &sizemode, &mode]() {
            switch(mode) {
            case PositionTopLeft:
                setMoveResizeGeometry(QRect(topleft, orig.bottomRight()));
                break;
            case PositionBottomRight:
                setMoveResizeGeometry(QRect(orig.topLeft(), bottomright));
                break;
            case PositionBottomLeft:
                setMoveResizeGeometry(QRect(QPoint(topleft.x(), orig.y()), QPoint(orig.right(), bottomright.y())));
                break;
            case PositionTopRight:
                setMoveResizeGeometry(QRect(QPoint(orig.x(), topleft.y()), QPoint(bottomright.x(), orig.bottom())));
                break;
            case PositionTop:
                setMoveResizeGeometry(QRect(QPoint(orig.left(), topleft.y()), orig.bottomRight()));
                sizemode = SizemodeFixedH; // try not to affect height
                break;
            case PositionBottom:
                setMoveResizeGeometry(QRect(orig.topLeft(), QPoint(orig.right(), bottomright.y())));
                sizemode = SizemodeFixedH;
                break;
            case PositionLeft:
                setMoveResizeGeometry(QRect(QPoint(topleft.x(), orig.top()), orig.bottomRight()));
                sizemode = SizemodeFixedW;
                break;
            case PositionRight:
                setMoveResizeGeometry(QRect(orig.topLeft(), QPoint(bottomright.x(), orig.bottom())));
                sizemode = SizemodeFixedW;
                break;
            case PositionCenter:
            default:
                abort();
                break;
            }
        };

        // first resize (without checking constrains), then snap, then check bounds, then check constrains
        calculateMoveResizeGeom();
        // adjust new size to snap to other windows/borders
        setMoveResizeGeometry(workspace()->adjustClientSize(this, moveResizeGeometry(), mode));

        if (!isUnrestrictedMoveResize()) {
            // Make sure the titlebar isn't behind a restricted area. We don't need to restrict
            // the other directions. If not visible enough, move the window to the closest valid
            // point. We bruteforce this by slowly moving the window back to its previous position
            QRegion availableArea(workspace()->clientArea(FullArea, -1, 0));   // On the screen
            availableArea -= workspace()->restrictedMoveArea(desktop());   // Strut areas
            bool transposed = false;
            int requiredPixels;
            QRect bTitleRect = titleBarRect(transposed, requiredPixels);
            int lastVisiblePixels = -1;
            QRect lastTry = moveResizeGeometry();
            bool titleFailed = false;
            for (;;) {
                const QRect titleRect(bTitleRect.translated(moveResizeGeometry().topLeft()));
                int visiblePixels = 0;
                int realVisiblePixels = 0;
                foreach (const QRect &rect, availableArea.rects()) {
                    const QRect r = rect & titleRect;
                    realVisiblePixels += r.width() * r.height();
                    if ((transposed && r.width() == titleRect.width()) || // Only the full size regions...
                        (!transposed && r.height() == titleRect.height())) // ...prevents long slim areas
                        visiblePixels += r.width() * r.height();
                }

                if (visiblePixels >= requiredPixels)
                    break; // We have reached a valid position

                if (realVisiblePixels <= lastVisiblePixels) {
                    if (titleFailed && realVisiblePixels < lastVisiblePixels)
                        break; // we won't become better
                    else {
                        if (!titleFailed)
                            setMoveResizeGeometry(lastTry);
                        titleFailed = true;
                    }
                }
                lastVisiblePixels = realVisiblePixels;
                QRect moveResizeGeom = moveResizeGeometry();
                lastTry = moveResizeGeom;

                // Not visible enough, move the window to the closest valid point. We bruteforce
                // this by slowly moving the window back to its previous position.
                // The geometry changes at up to two edges, the one with the title (if) shall take
                // precedence. The opposing edge has no impact on visiblePixels and only one of
                // the adjacent can alter at a time, ie. it's enough to ignore adjacent edges
                // if the title edge altered
                bool leftChanged  = previousMoveResizeGeom.left()   != moveResizeGeom.left();
                bool rightChanged = previousMoveResizeGeom.right()  != moveResizeGeom.right();
                bool topChanged   = previousMoveResizeGeom.top()    != moveResizeGeom.top();
                bool btmChanged   = previousMoveResizeGeom.bottom() != moveResizeGeom.bottom();
                auto fixChangedState = [titleFailed](bool &major, bool &counter, bool &ad1, bool &ad2) {
                    counter = false;
                    if (titleFailed)
                        major = false;
                    if (major)
                        ad1 = ad2 = false;
                };
                switch (titlebarPosition()) {
                default:
                case PositionTop:
                    fixChangedState(topChanged, btmChanged, leftChanged, rightChanged);
                    break;
                case PositionLeft:
                    fixChangedState(leftChanged, rightChanged, topChanged, btmChanged);
                    break;
                case PositionBottom:
                    fixChangedState(btmChanged, topChanged, leftChanged, rightChanged);
                    break;
                case PositionRight:
                    fixChangedState(rightChanged, leftChanged, topChanged, btmChanged);
                    break;
                }
                if (topChanged)
                    moveResizeGeom.setTop(moveResizeGeom.y() + sign(previousMoveResizeGeom.y() - moveResizeGeom.y()));
                else if (leftChanged)
                    moveResizeGeom.setLeft(moveResizeGeom.x() + sign(previousMoveResizeGeom.x() - moveResizeGeom.x()));
                else if (btmChanged)
                    moveResizeGeom.setBottom(moveResizeGeom.bottom() + sign(previousMoveResizeGeom.bottom() - moveResizeGeom.bottom()));
                else if (rightChanged)
                    moveResizeGeom.setRight(moveResizeGeom.right() + sign(previousMoveResizeGeom.right() - moveResizeGeom.right()));
                else
                    break; // no position changed - that's certainly not good
                setMoveResizeGeometry(moveResizeGeom);
            }
        }

        // Always obey size hints, even when in "unrestricted" mode
        QSize size = adjustedSize(moveResizeGeometry().size(), sizemode);
        // the new topleft and bottomright corners (after checking size constrains), if they'll be needed
        topleft = QPoint(moveResizeGeometry().right() - size.width() + 1, moveResizeGeometry().bottom() - size.height() + 1);
        bottomright = QPoint(moveResizeGeometry().left() + size.width() - 1, moveResizeGeometry().top() + size.height() - 1);
        orig = moveResizeGeometry();

        // if aspect ratios are specified, both dimensions may change.
        // Therefore grow to the right/bottom if needed.
        // TODO it should probably obey gravity rather than always using right/bottom ?
        if (sizemode == SizemodeFixedH)
            orig.setRight(bottomright.x());
        else if (sizemode == SizemodeFixedW)
            orig.setBottom(bottomright.y());

        calculateMoveResizeGeom();

        if (moveResizeGeometry().size() != previousMoveResizeGeom.size())
            update = true;
    } else if (isMove()) {
        assert(mode == PositionCenter);
        if (!isMovable()) { // isMovableAcrossScreens() must have been true to get here
            // Special moving of maximized windows on Xinerama screens
            int screen = screens()->number(globalPos);
            if (isFullScreen())
                setMoveResizeGeometry(workspace()->clientArea(FullScreenArea, screen, 0));
            else {
                QRect moveResizeGeom = workspace()->clientArea(MaximizeArea, screen, 0);
                QSize adjSize = adjustedSize(moveResizeGeom.size(), SizemodeMax);
                if (adjSize != moveResizeGeom.size()) {
                    QRect r(moveResizeGeom);
                    moveResizeGeom.setSize(adjSize);
                    moveResizeGeom.moveCenter(r.center());
                }
                setMoveResizeGeometry(moveResizeGeom);
            }
        } else {
            // first move, then snap, then check bounds
            QRect moveResizeGeom = moveResizeGeometry();
            moveResizeGeom.moveTopLeft(topleft);
            moveResizeGeom.moveTopLeft(workspace()->adjustClientPosition(this, moveResizeGeom.topLeft(),
                                       isUnrestrictedMoveResize()));
            setMoveResizeGeometry(moveResizeGeom);

            if (!isUnrestrictedMoveResize()) {
                const QRegion strut = workspace()->restrictedMoveArea(desktop());   // Strut areas
                QRegion availableArea(workspace()->clientArea(FullArea, -1, 0));   // On the screen
                availableArea -= strut;   // Strut areas
                bool transposed = false;
                int requiredPixels;
                QRect bTitleRect = titleBarRect(transposed, requiredPixels);
                for (;;) {
                    QRect moveResizeGeom = moveResizeGeometry();
                    const QRect titleRect(bTitleRect.translated(moveResizeGeom.topLeft()));
                    int visiblePixels = 0;
                    foreach (const QRect &rect, availableArea.rects()) {
                        const QRect r = rect & titleRect;
                        if ((transposed && r.width() == titleRect.width()) || // Only the full size regions...
                            (!transposed && r.height() == titleRect.height())) // ...prevents long slim areas
                            visiblePixels += r.width() * r.height();
                    }
                    if (visiblePixels >= requiredPixels)
                        break; // We have reached a valid position

                    // (esp.) if there're more screens with different struts (panels) it the titlebar
                    // will be movable outside the movearea (covering one of the panels) until it
                    // crosses the panel "too much" (not enough visiblePixels) and then stucks because
                    // it's usually only pushed by 1px to either direction
                    // so we first check whether we intersect suc strut and move the window below it
                    // immediately (it's still possible to hit the visiblePixels >= titlebarArea break
                    // by moving the window slightly downwards, but it won't stuck)
                    // see bug #274466
                    // and bug #301805 for why we can't just match the titlearea against the screen
                    if (screens()->count() > 1) { // optimization
                        // TODO: could be useful on partial screen struts (half-width panels etc.)
                        int newTitleTop = -1;
                        foreach (const QRect &r, strut.rects()) {
                            if (r.top() == 0 && r.width() > r.height() && // "top panel"
                                r.intersects(moveResizeGeom) && moveResizeGeom.top() < r.bottom()) {
                                newTitleTop = r.bottom() + 1;
                                break;
                            }
                        }
                        if (newTitleTop > -1) {
                            moveResizeGeom.moveTop(newTitleTop); // invalid position, possibly on screen change
                            setMoveResizeGeometry(moveResizeGeom);
                            break;
                        }
                    }

                    int dx = sign(previousMoveResizeGeom.x() - moveResizeGeom.x()),
                        dy = sign(previousMoveResizeGeom.y() - moveResizeGeom.y());
                    if (visiblePixels && dx) // means there's no full width cap -> favor horizontally
                        dy = 0;
                    else if (dy)
                        dx = 0;

                    // Move it back
                    moveResizeGeom.translate(dx, dy);
                    setMoveResizeGeometry(moveResizeGeom);

                    if (moveResizeGeom == previousMoveResizeGeom) {
                        break; // Prevent lockup
                    }
                }
            }
        }
        if (moveResizeGeometry().topLeft() != previousMoveResizeGeom.topLeft())
            update = true;
    } else
        abort();

    if (!update)
        return;

    if (isResize() && !haveResizeEffect()) {
        doResizeSync();
    } else
        performMoveResize();

    if (isMove()) {
        ScreenEdges::self()->check(globalPos, QDateTime::fromMSecsSinceEpoch(xTime()));
    }
}

void Client::doResizeSync()
{
    if (!syncRequest.timeout) {
        syncRequest.timeout = new QTimer(this);
        connect(syncRequest.timeout, &QTimer::timeout, this, &Client::performMoveResize);
        syncRequest.timeout->setSingleShot(true);
    }
    if (syncRequest.counter != XCB_NONE) {
        syncRequest.timeout->start(250);
        sendSyncRequest();
    } else {                            // for clients not supporting the XSYNC protocol, we
        syncRequest.isPending = true;   // limit the resizes to 30Hz to take pointless load from X11
        syncRequest.timeout->start(33); // and the client, the mouse is still moved at full speed
    }                                   // and no human can control faster resizes anyway
    const QRect &moveResizeGeom = moveResizeGeometry();
    m_client.setGeometry(0, 0, moveResizeGeom.width() - (borderLeft() + borderRight()), moveResizeGeom.height() - (borderTop() + borderBottom()));
}

void AbstractClient::performMoveResize()
{
    const QRect &moveResizeGeom = moveResizeGeometry();
    if (isMove() || (isResize() && !haveResizeEffect())) {
        setGeometry(moveResizeGeom);
    }
    doPerformMoveResize();
    if (isResize())
        addRepaintFull();
    positionGeometryTip();
    emit clientStepUserMovedResized(this, moveResizeGeom);
}

void Client::doPerformMoveResize()
{
    if (syncRequest.counter == XCB_NONE)   // client w/o XSYNC support. allow the next resize event
        syncRequest.isPending = false; // NEVER do this for clients with a valid counter
                                       // (leads to sync request races in some clients)
}

void AbstractClient::setElectricBorderMode(QuickTileMode mode)
{
    if (mode != QuickTileMode(QuickTileFlag::Maximize)) {
        // sanitize the mode, ie. simplify "invalid" combinations
        if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Horizontal))
            mode &= ~QuickTileMode(QuickTileFlag::Horizontal);
        if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Vertical))
            mode &= ~QuickTileMode(QuickTileFlag::Vertical);
    }
    m_electricMode = mode;
}

void AbstractClient::setElectricBorderMaximizing(bool maximizing)
{
    m_electricMaximizing = maximizing;
    if (maximizing)
        outline()->show(electricBorderMaximizeGeometry(Cursor::pos(), desktop()), moveResizeGeometry());
    else
        outline()->hide();
    elevate(maximizing);
}

QRect AbstractClient::electricBorderMaximizeGeometry(QPoint pos, int desktop)
{
    if (electricBorderMode() == QuickTileMode(QuickTileFlag::Maximize)) {
        if (maximizeMode() == MaximizeFull)
            return geometryRestore();
        else
            return workspace()->clientArea(MaximizeArea, pos, desktop);
    }

    QRect ret = workspace()->clientArea(MaximizeArea, pos, desktop);
    if (electricBorderMode() & QuickTileFlag::Left)
        ret.setRight(ret.left()+ret.width()/2 - 1);
    else if (electricBorderMode() & QuickTileFlag::Right)
        ret.setLeft(ret.right()-(ret.width()-ret.width()/2) + 1);
    if (electricBorderMode() & QuickTileFlag::Top)
        ret.setBottom(ret.top()+ret.height()/2 - 1);
    else if (electricBorderMode() & QuickTileFlag::Bottom)
        ret.setTop(ret.bottom()-(ret.height()-ret.height()/2) + 1);

    return ret;
}

void AbstractClient::setQuickTileMode(QuickTileMode mode, bool keyboard)
{
    // Only allow quick tile on a regular or maximized window
    if (!isResizable() && maximizeMode() != MaximizeFull)
        return;

    workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event

    GeometryUpdatesBlocker blocker(this);

    if (mode == QuickTileMode(QuickTileFlag::Maximize)) {
        TabSynchronizer syncer(this, TabGroup::QuickTile|TabGroup::Geometry|TabGroup::Maximized);
        m_quickTileMode = int(QuickTileFlag::None);
        if (maximizeMode() == MaximizeFull) {
            setMaximize(false, false);
        } else {
            QRect prev_geom_restore = geometryRestore(); // setMaximize() would set moveResizeGeom as geom_restore
            m_quickTileMode = int(QuickTileFlag::Maximize);
            setMaximize(true, true);
            QRect clientArea = workspace()->clientArea(MaximizeArea, this);
            if (geometry().top() != clientArea.top()) {
                QRect r(geometry());
                r.moveTop(clientArea.top());
                setGeometry(r);
            }
            setGeometryRestore(prev_geom_restore);
        }
        emit quickTileModeChanged();
        return;
    }

    // sanitize the mode, ie. simplify "invalid" combinations
    if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Horizontal))
        mode &= ~QuickTileMode(QuickTileFlag::Horizontal);
    if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Vertical))
        mode &= ~QuickTileMode(QuickTileFlag::Vertical);

    setElectricBorderMode(mode); // used by ::electricBorderMaximizeGeometry(.)

    // restore from maximized so that it is possible to tile maximized windows with one hit or by dragging
    if (maximizeMode() != MaximizeRestore) {

        TabSynchronizer syncer(this, TabGroup::QuickTile|TabGroup::Geometry|TabGroup::Maximized);

        if (mode != QuickTileMode(QuickTileFlag::None)) {
            // decorations may turn off some borders when tiled
            const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;
            m_quickTileMode = int(QuickTileFlag::None); // Temporary, so the maximize code doesn't get all confused

            setMaximize(false, false);

            setGeometry(electricBorderMaximizeGeometry(keyboard ? geometry().center() : Cursor::pos(), desktop()), geom_mode);
            // Store the mode change
            m_quickTileMode = mode;
        } else {
            m_quickTileMode = mode;
            setMaximize(false, false);
        }

        emit quickTileModeChanged();

        return;
    }

    if (mode != QuickTileMode(QuickTileFlag::None)) {
        TabSynchronizer syncer(this, TabGroup::QuickTile|TabGroup::Geometry);

        QPoint whichScreen = keyboard ? geometry().center() : Cursor::pos();

        // If trying to tile to the side that the window is already tiled to move the window to the next
        // screen if it exists, otherwise toggle the mode (set QuickTileFlag::None)
        if (quickTileMode() == mode) {
            const int numScreens = screens()->count();
            const int curScreen = screen();
            int nextScreen = curScreen;
            QVarLengthArray<QRect> screens(numScreens);
            for (int i = 0; i < numScreens; ++i)   // Cache
                screens[i] = Screens::self()->geometry(i);
            for (int i = 0; i < numScreens; ++i) {

                if (i == curScreen)
                    continue;

                if (screens[i].bottom() <= screens[curScreen].top() || screens[i].top() >= screens[curScreen].bottom())
                    continue; // not in horizontal line

                const int x = screens[i].center().x();
                if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Left)) {
                    if (x >= screens[curScreen].center().x() || (curScreen != nextScreen && x <= screens[nextScreen].center().x()))
                        continue; // not left of current or more left then found next
                } else if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Right)) {
                    if (x <= screens[curScreen].center().x() || (curScreen != nextScreen && x >= screens[nextScreen].center().x()))
                        continue; // not right of current or more right then found next
                }

                nextScreen = i;
            }

            if (nextScreen == curScreen) {
                mode = QuickTileFlag::None; // No other screens, toggle tiling
            } else {
                // Move to other screen
                setGeometry(geometryRestore().translated(screens[nextScreen].topLeft() - screens[curScreen].topLeft()));
                whichScreen = screens[nextScreen].center();

                // Swap sides
                if (mode & QuickTileFlag::Horizontal) {
                    mode = (~mode & QuickTileFlag::Horizontal) | (mode & QuickTileFlag::Vertical);
                }
            }
            setElectricBorderMode(mode); // used by ::electricBorderMaximizeGeometry(.)
        } else if (quickTileMode() == QuickTileMode(QuickTileFlag::None)) {
            // Not coming out of an existing tile, not shifting monitors, we're setting a brand new tile.
            // Store geometry first, so we can go out of this tile later.
            setGeometryRestore(geometry());
        }

        if (mode != QuickTileMode(QuickTileFlag::None)) {
            m_quickTileMode = mode;
            // decorations may turn off some borders when tiled
            const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;
            // Temporary, so the maximize code doesn't get all confused
            m_quickTileMode = int(QuickTileFlag::None);
            setGeometry(electricBorderMaximizeGeometry(whichScreen, desktop()), geom_mode);
        }

        // Store the mode change
        m_quickTileMode = mode;
    }

    if (mode == QuickTileMode(QuickTileFlag::None)) {
        TabSynchronizer syncer(this, TabGroup::QuickTile|TabGroup::Geometry);

        m_quickTileMode = int(QuickTileFlag::None);
        // Untiling, so just restore geometry, and we're done.
        if (!geometryRestore().isValid()) // invalid if we started maximized and wait for placement
            setGeometryRestore(geometry());
        // decorations may turn off some borders when tiled
        const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;
        setGeometry(geometryRestore(), geom_mode);
        checkWorkspacePosition(); // Just in case it's a different screen
    }
    emit quickTileModeChanged();
}

void AbstractClient::sendToScreen(int newScreen)
{
    newScreen = rules()->checkScreen(newScreen);
    if (isActive()) {
        screens()->setCurrent(newScreen);
        // might impact the layer of a fullscreen window
        foreach (AbstractClient *cc, workspace()->allClientList()) {
            if (cc->isFullScreen() && cc->screen() == newScreen) {
                cc->updateLayer();
            }
        }
    }
    if (screen() == newScreen)   // Don't use isOnScreen(), that's true even when only partially
        return;

    GeometryUpdatesBlocker blocker(this);

    // operating on the maximized / quicktiled window would leave the old geom_restore behind,
    // so we clear the state first
    MaximizeMode maxMode = maximizeMode();
    QuickTileMode qtMode = quickTileMode();
    if (maxMode != MaximizeRestore)
        maximize(MaximizeRestore);
    if (qtMode != QuickTileMode(QuickTileFlag::None))
        setQuickTileMode(QuickTileFlag::None, true);

    QRect oldScreenArea = workspace()->clientArea(MaximizeArea, this);
    QRect screenArea = workspace()->clientArea(MaximizeArea, newScreen, desktop());

    // the window can have its center so that the position correction moves the new center onto
    // the old screen, what will tile it where it is. Ie. the screen is not changed
    // this happens esp. with electric border quicktiling
    if (qtMode != QuickTileMode(QuickTileFlag::None))
        keepInArea(oldScreenArea);

    QRect oldGeom = geometry();
    QRect newGeom = oldGeom;
    // move the window to have the same relative position to the center of the screen
    // (i.e. one near the middle of the right edge will also end up near the middle of the right edge)
    QPoint center = newGeom.center() - oldScreenArea.center();
    center.setX(center.x() * screenArea.width() / oldScreenArea.width());
    center.setY(center.y() * screenArea.height() / oldScreenArea.height());
    center += screenArea.center();
    newGeom.moveCenter(center);
    setGeometry(newGeom);

    // If the window was inside the old screen area, explicitly make sure its inside also the new screen area.
    // Calling checkWorkspacePosition() should ensure that, but when moving to a small screen the window could
    // be big enough to overlap outside of the new screen area, making struts from other screens come into effect,
    // which could alter the resulting geometry.
    if (oldScreenArea.contains(oldGeom)) {
        keepInArea(screenArea);
    }

    // align geom_restore - checkWorkspacePosition operates on it
    setGeometryRestore(geometry());

    checkWorkspacePosition(oldGeom);

    // re-align geom_restore to constrained geometry
    setGeometryRestore(geometry());

    // finally reset special states
    // NOTICE that MaximizeRestore/QuickTileFlag::None checks are required.
    // eg. setting QuickTileFlag::None would break maximization
    if (maxMode != MaximizeRestore)
        maximize(maxMode);
    if (qtMode != QuickTileMode(QuickTileFlag::None) && qtMode != quickTileMode())
        setQuickTileMode(qtMode, true);

    auto tso = workspace()->ensureStackingOrder(transients());
    for (auto it = tso.constBegin(), end = tso.constEnd(); it != end; ++it)
        (*it)->sendToScreen(newScreen);
}

} // namespace
