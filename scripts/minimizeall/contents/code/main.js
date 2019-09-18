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

function minimizeAllWindows() {
    var allClients = workspace.clientList();
    var relevantClients = [];
    var minimize = false;

    for (var i = 0; i < allClients.length; ++i) {
        if (!isRelevant(allClients[i])) {
            continue;
        }
        if (!allClients[i].minimized) {
            minimize = true;
        }
        relevantClients.push(allClients[i]);
    }

    for (var i = 0; i < relevantClients.length; ++i) {
        var wasMinimizedByScript = relevantClients[i].minimizedByScript;
        delete relevantClients[i].minimizedByScript;

        if (minimize) {
            if (relevantClients[i].minimized) {
                continue;
            }
            relevantClients[i].minimized = true;
            relevantClients[i].minimizedByScript = true;
        } else {
            if (!wasMinimizedByScript) {
                continue;
            }
            relevantClients[i].minimized = false;
        }
    }
}

function init() {
    for (var i in registeredBorders) {
        unregisterScreenEdge(registeredBorders[i]);
    }

    registeredBorders = [];

    var borders = readConfig("BorderActivate", "").toString().split(",");
    for (var i in borders) {
        var border = parseInt(borders[i]);
        if (isFinite(border)) {
            registeredBorders.push(border);
            registerScreenEdge(border, minimizeAllWindows);
        }
    }
}

options.configChanged.connect(init);

registerShortcut("MinimizeAll", "MinimizeAll", "Meta+Shift+D", minimizeAllWindows);
init();
