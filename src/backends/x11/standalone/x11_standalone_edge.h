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
#pragma once
#include "screenedge.h"
#include "utils/xcbutils.h"

namespace KWin
{

class WindowBasedEdge : public Edge
{
    Q_OBJECT
public:
    explicit WindowBasedEdge(ScreenEdges *parent);
    ~WindowBasedEdge() override;

    quint32 window() const override;
    /**
     * The approach window is a special window to notice when get close to the screen border but
     * not yet triggering the border.
     */
    quint32 approachWindow() const override;

protected:
    void doGeometryUpdate() override;
    void doActivate() override;
    void doDeactivate() override;
    void doStartApproaching() override;
    void doStopApproaching() override;
    void doUpdateBlocking() override;

private:
    void createWindow();
    void createApproachWindow();
    Xcb::Window m_window;
    Xcb::Window m_approachWindow;
    QMetaObject::Connection m_cursorPollingConnection;
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
