/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2014 Thomas Lübking <thomas.luebking@gmail.com>

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

var registeredBorders = [];

function isRelevant(client) {
    return client.minimizable &&
           (client.onAllDesktops || client.desktop === workspace.currentDesktop);
           (!client.activities.length || client.activities.indexOf(workspace.currentActivity.toString()) > -1);
}

var minimizeAllWindows = function() {
    var allClients = workspace.clientList();
    var clients = [];
    var minimize = false;
    for (var i = 0; i < allClients.length; ++i) {
        if (isRelevant(allClients[i])) {
            clients.push(allClients[i]);
            if (!allClients[i].minimized && allClients[i].minimizedForMinimizeAll !== true) {
                minimize = true;
            }
        }
    }
    for (var i = 0; i < clients.length; ++i) {
        if ((minimize == clients[i].minimized) || // no change required at all
            (!minimize && clients[i].minimizedForMinimizeAll !== true)) { // unminimize, but not this one
            delete clients[i].minimizedForMinimizeAll;
            continue;
        }
        clients[i].minimized = minimize;
        clients[i].minimizedForMinimizeAll = minimize;
    }
    clients = [];
}

function init() {
    for (var i in registeredBorders) {
        unregisterScreenEdge(registeredBorders[i]);
    }

    registeredBorders = [];

    var borders = readConfig("BorderActivate", "").toString().split(",");
    for (var i in borders) {
        registeredBorders.push(borders[i]);
        registerScreenEdge(borders[i], minimizeAllWindows);
    }
}

options.configChanged.connect(init);

registerShortcut("MinimizeAll", "MinimizeAll", "Meta+Shift+D", minimizeAllWindows);
init();
