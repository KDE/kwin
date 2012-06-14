/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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

#ifndef KWIN_SCREENEDGE_H
#define KWIN_SCREENEDGE_H
#include <QtCore/QObject>
#include <QtCore/QVector>
#include "kwinglobals.h"


namespace KWin {

/**
 * @short This class is used to handle the screen edges
 * Screen Edge Window management. Screen Edges allow a user to change the virtual
 * desktop or activate other features by moving the mouse pointer to the borders or
 * corners of the screen. Technically this is done with input only windows.
 *
 * @author Arthur Arlt
 * @since 4.8
 */
class ScreenEdge : public QObject {
    Q_OBJECT
public:
    ScreenEdge();
    ~ScreenEdge();
    /**
     * Initialize the screen edges.
     */
    void init();
    /**
     * Check, if a screen edge is entered and trigger the appropriate action
     * if one is enabled for the current region and the timeout is satisfied
     * @param pos the position of the mouse pointer
     * @param now the time when the function is called
     * @param forceNoPushBack needs to be called to workaround some DnD clients, don't use unless you want to chek on a DnD event
     */
    void check(const QPoint& pos, Time now, bool forceNoPushBack = false);
    /**
     * Restore the size of the specified screen edges
     * @param border the screen edge to restore the size of
     */
    void restoreSize(ElectricBorder border);
    /**
     * Mark the specified screen edge as reserved in m_screenEdgeReserved
     * @param border the screen edge to mark as reserved
     */
    void reserve(ElectricBorder border);
    /**
     * Mark the specified screen edge as unreserved in m_screenEdgeReserved
     * @param border the screen edge to mark as unreserved
     */
    void unreserve(ElectricBorder border);
    /**
     * Reserve actions for screen edges, if reserve is true. Unreserve otherwise.
     * @param reserve indicated weather actions should be reserved or unreseved
     */
    void reserveActions(bool isToReserve);
    /**
     * Reserve desktop switching for screen edges, if reserve is true. Unreserve otherwise.
     * @param reserve indicated weather desktop switching should be reserved or unreseved
     */
    void reserveDesktopSwitching(bool isToReserve, Qt::Orientations o);
    /**
     * Raise electric border windows to the real top of the screen. We only need
     * to do this if an effect input window is active.
     */
    void ensureOnTop();
    /**
     * Raise FOREIGN border windows to the real top of the screen. We usually need
     * to do this after an effect input window was active.
     */
    void raisePanelProxies();
    /**
    * Called when the user entered an electric border with the mouse.
    * It may switch to another virtual desktop.
    * @param e the X event which is passed to this method.
    */
    bool isEntered(XEvent * e);
    /**
     * Returns a QVector of all existing screen edge windows
     * @return all existing screen edge windows in a QVector
     */
    const QVector< Window >& windows();
public Q_SLOTS:
    /**
     * Update the screen edge windows. Add new ones if the user specified
     * a new action or enabled desktop switching. Remove, if user deleted
     * actions or disabled desktop switching.
     */
    void update(bool force=false);
Q_SIGNALS:
    /**
     * Emitted when the @p border got activated and there is neither an effect nor a global
     * action configured for this @p border.
     * @param border The border which got activated
     **/
    void activated(ElectricBorder border);
private:
    /**
     * Switch the desktop if desktop switching is enabled and a screen edge
     * is entered to trigger this action.
     */
    void switchDesktop(ElectricBorder border, const QPoint& pos);

    QVector< Window > m_screenEdgeWindows;
    QVector< int > m_screenEdgeReserved; // Corners/edges used by something
    ElectricBorder m_currentScreenEdge;
    int m_screenEdgeLeft;
    int m_screenEdgeRight;
    int m_screenEdgeTop;
    int m_screenEdgeBottom;
    Time m_screenEdgeTimeFirst;
    Time m_screenEdgeTimeLast;
    Time m_screenEdgeTimeLastTrigger;
    QPoint m_screenEdgePushPoint;
};
}
#endif // KWIN_SCREENEDGE_H
