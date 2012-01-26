/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>
Copyright (C) 2011, 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#include "workspace_wrapper.h"
#include "../client.h"

namespace KWin {

WorkspaceWrapper::WorkspaceWrapper(QObject* parent) : QObject(parent)
{
    KWin::Workspace *ws = KWin::Workspace::self();
    connect(ws, SIGNAL(desktopPresenceChanged(KWin::Client*,int)), SIGNAL(desktopPresenceChanged(KWin::Client*,int)));
    connect(ws, SIGNAL(currentDesktopChanged(int)), SIGNAL(currentDesktopChanged(int)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), SIGNAL(clientAdded(KWin::Client*)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), SLOT(setupClientConnections(KWin::Client*)));
    connect(ws, SIGNAL(clientRemoved(KWin::Client*)), SIGNAL(clientRemoved(KWin::Client*)));
    connect(ws, SIGNAL(clientActivated(KWin::Client*)), SIGNAL(clientActivated(KWin::Client*)));
    connect(ws, SIGNAL(numberDesktopsChanged(int)), SIGNAL(numberDesktopsChanged(int)));
    foreach (KWin::Client *client, ws->clientList()) {
        setupClientConnections(client);
    }
}

#define GETTERSETTER( rettype, getterName, setterName ) \
rettype WorkspaceWrapper::getterName( ) const { \
    return Workspace::self()->getterName(); \
} \
void WorkspaceWrapper::setterName( rettype val ) { \
    Workspace::self()->setterName( val ); \
}

GETTERSETTER(int, numberOfDesktops, setNumberOfDesktops)
GETTERSETTER(int, currentDesktop, setCurrentDesktop)

#undef GETTERSETTER

#define GETTER( rettype, getterName ) \
rettype WorkspaceWrapper::getterName( ) const { \
    return Workspace::self()->getterName(); \
}
GETTER(KWin::Client*, activeClient)
GETTER(QList< KWin::Client* >, clientList)
GETTER(int, workspaceWidth)
GETTER(int, workspaceHeight)
GETTER(QSize, desktopGridSize)
GETTER(int, desktopGridWidth)
GETTER(int, desktopGridHeight)

#undef GETTER

void WorkspaceWrapper::setActiveClient(KWin::Client* client)
{
    KWin::Workspace::self()->activateClient(client);
}

QSize WorkspaceWrapper::workspaceSize() const
{
    return QSize(workspaceWidth(), workspaceHeight());
}

QSize WorkspaceWrapper::displaySize() const
{
    return QSize(KWin::displayWidth(), KWin::displayHeight());
}

int WorkspaceWrapper::displayWidth() const
{
    return KWin::displayWidth();
}

int WorkspaceWrapper::displayHeight() const
{
    return KWin::displayWidth();
}

void WorkspaceWrapper::setupClientConnections(KWin::Client *client)
{
    connect(client, SIGNAL(clientMinimized(KWin::Client*,bool)), SIGNAL(clientMinimized(KWin::Client*)));
    connect(client, SIGNAL(clientUnminimized(KWin::Client*,bool)), SIGNAL(clientUnminimized(KWin::Client*)));
    connect(client, SIGNAL(clientManaging(KWin::Client*)), SIGNAL(clientManaging(KWin::Client*)));
    connect(client, SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)), SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)));
    connect(client, SIGNAL(clientMaximizedStateChanged(KWin::Client*,bool,bool)), SIGNAL(clientMaximizeSet(KWin::Client*,bool,bool)));
}

} // KWin
