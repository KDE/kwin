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
#include "edge.h"
#include "atoms.h"
#include "cursor.h"

namespace KWin
{

WindowBasedEdge::WindowBasedEdge(ScreenEdges *parent)
    : Edge(parent)
    , m_window(XCB_WINDOW_NONE)
    , m_approachWindow(XCB_WINDOW_NONE)
{
}

WindowBasedEdge::~WindowBasedEdge()
{
}

void WindowBasedEdge::doActivate()
{
    createWindow();
    createApproachWindow();
    doUpdateBlocking();
}

void WindowBasedEdge::doDeactivate()
{
    m_window.reset();
    m_approachWindow.reset();
}

void WindowBasedEdge::createWindow()
{
    if (m_window.isValid()) {
        return;
    }
    const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    const uint32_t values[] = {
        true,
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION
    };
    m_window.create(geometry(), XCB_WINDOW_CLASS_INPUT_ONLY, mask, values);
    m_window.map();
    // Set XdndAware on the windows, so that DND enter events are received (#86998)
    xcb_atom_t version = 4; // XDND version
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, m_window,
                        atoms->xdnd_aware, XCB_ATOM_ATOM, 32, 1, (unsigned char*)(&version));
}

void WindowBasedEdge::createApproachWindow()
{
    if (!activatesForPointer()) {
        return;
    }
    if (m_approachWindow.isValid()) {
        return;
    }
    if (!approachGeometry().isValid()) {
        return;
    }
    const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    const uint32_t values[] = {
        true,
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION
    };
    m_approachWindow.create(approachGeometry(), XCB_WINDOW_CLASS_INPUT_ONLY, mask, values);
    m_approachWindow.map();
}

void WindowBasedEdge::doGeometryUpdate()
{
    m_window.setGeometry(geometry());
    if (m_approachWindow.isValid()) {
        m_approachWindow.setGeometry(approachGeometry());
    }
}

void WindowBasedEdge::doStartApproaching()
{
    if (!activatesForPointer()) {
        return;
    }
    m_approachWindow.unmap();
    Cursor *cursor = Cursor::self();
    connect(cursor, SIGNAL(posChanged(QPoint)), SLOT(updateApproaching(QPoint)));
    cursor->startMousePolling();
}

void WindowBasedEdge::doStopApproaching()
{
    if (!activatesForPointer()) {
        return;
    }
    Cursor *cursor = Cursor::self();
    disconnect(cursor, SIGNAL(posChanged(QPoint)), this, SLOT(updateApproaching(QPoint)));
    cursor->stopMousePolling();
    m_approachWindow.map();
}

void WindowBasedEdge::doUpdateBlocking()
{
    if (!isReserved()) {
        return;
    }
    if (isBlocked()) {
        m_window.unmap();
        m_approachWindow.unmap();
    } else {
        m_window.map();
        m_approachWindow.map();
    }
}

}
