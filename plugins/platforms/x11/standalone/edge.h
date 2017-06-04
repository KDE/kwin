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
#ifndef KWIN_EDGE_H
#define KWIN_EDGE_H
#include "screenedge.h"
#include "xcbutils.h"

namespace KWin
{

class WindowBasedEdge : public Edge
{
    Q_OBJECT
public:
    explicit WindowBasedEdge(ScreenEdges *parent);
    virtual ~WindowBasedEdge();

    quint32 window() const override;
    /**
     * The approach window is a special window to notice when get close to the screen border but
     * not yet triggering the border.
     **/
    quint32 approachWindow() const override;

protected:
    void doGeometryUpdate() override;
    virtual void doActivate() override;
    virtual void doDeactivate() override;
    void doStartApproaching() override;
    void doStopApproaching() override;
    void doUpdateBlocking() override;

private:
    void createWindow();
    void createApproachWindow();
    Xcb::Window m_window;
    Xcb::Window m_approachWindow;
};

inline quint32 WindowBasedEdge::window() const
{
    return m_window;
}

inline quint32 WindowBasedEdge::approachWindow() const
{
    return m_approachWindow;
}

}

#endif
