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

#include "workspace.h"
#include "../client.h"

SWrapper::Workspace::Workspace(QObject* parent) : QObject(parent)
{
    KWin::Workspace *ws = KWin::Workspace::self();
    connect(ws, SIGNAL(desktopPresenceChanged(KWin::Client*,int)), SIGNAL(desktopPresenceChanged(KWin::Client*,int)));
    connect(ws, SIGNAL(currentDesktopChanged(int)), SIGNAL(currentDesktopChanged(int)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), SIGNAL(clientAdded(KWin::Client*)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), SLOT(setupClientConnections(KWin::Client*)));
    connect(ws, SIGNAL(clientRemoved(KWin::Client*)), SIGNAL(clientRemoved(KWin::Client*)));
    connect(ws, SIGNAL(clientActivated(KWin::Client*)), SIGNAL(clientActivated(KWin::Client*)));
    foreach (KWin::Client *client, ws->clientList()) {
        setupClientConnections(client);
    }
}

QList< KWin::Client* > SWrapper::Workspace::clientList() const
{
    return KWin::Workspace::self()->clientList();
}

KWin::Client* SWrapper::Workspace::activeClient()
{
    return KWin::Workspace::self()->activeClient();
}

void SWrapper::Workspace::setActiveClient(KWin::Client* client)
{
    KWin::Workspace::self()->activateClient(client);
}

void SWrapper::Workspace::setCurrentDesktop(int desktop)
{
    KWin::Workspace::self()->setCurrentDesktop(desktop);
}

int SWrapper::Workspace::workspaceWidth() const
{
    return KWin::Workspace::self()->workspaceWidth();
}

int SWrapper::Workspace::workspaceHeight() const
{
    return KWin::Workspace::self()->workspaceHeight();
}

QSize SWrapper::Workspace::workspaceSize() const
{
    return QSize(workspaceWidth(), workspaceHeight());
}

QSize SWrapper::Workspace::desktopGridSize() const
{
    return KWin::Workspace::self()->desktopGridSize();
}

int SWrapper::Workspace::desktopGridWidth() const
{
    return KWin::Workspace::self()->desktopGridWidth();
}

int SWrapper::Workspace::desktopGridHeight() const
{
    return KWin::Workspace::self()->desktopGridHeight();
}

int SWrapper::Workspace::currentDesktop() const
{
    return KWin::Workspace::self()->currentDesktop();
}

void SWrapper::Workspace::setupClientConnections(KWin::Client *client)
{
    connect(client, SIGNAL(clientMinimized(KWin::Client*,bool)), SIGNAL(clientMinimized(KWin::Client*)));
    connect(client, SIGNAL(clientUnminimized(KWin::Client*,bool)), SIGNAL(clientUnminimized(KWin::Client*)));
    connect(client, SIGNAL(clientManaging(KWin::Client*)), SIGNAL(clientManaging(KWin::Client*)));
    connect(client, SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)), SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)));
    connect(client, SIGNAL(clientMaximizedStateChanged(KWin::Client*,bool,bool)), SIGNAL(clientMaximizeSet(KWin::Client*,bool,bool)));
}
