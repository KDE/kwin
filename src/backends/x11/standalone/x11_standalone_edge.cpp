/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    Since the functionality provided in this class has been moved from
    class Workspace, it is not clear who exactly has written the code.
    The list below contains the copyright holders of the class Workspace.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_edge.h"
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
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION};
    m_window.create(geometry(), XCB_WINDOW_CLASS_INPUT_ONLY, mask, values);
    m_window.map();
    // Set XdndAware on the windows, so that DND enter events are received (#86998)
    xcb_atom_t version = 4; // XDND version
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, m_window,
                        atoms->xdnd_aware, XCB_ATOM_ATOM, 32, 1, (unsigned char *)(&version));
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
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION};
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
    Cursor *cursor = Cursors::self()->mouse();
    m_cursorPollingConnection = connect(cursor, &Cursor::posChanged, this, &WindowBasedEdge::updateApproaching);
    cursor->startMousePolling();
}

void WindowBasedEdge::doStopApproaching()
{
    if (!m_cursorPollingConnection) {
        return;
    }
    disconnect(m_cursorPollingConnection);
    m_cursorPollingConnection = QMetaObject::Connection();
    Cursors::self()->mouse()->stopMousePolling();
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
