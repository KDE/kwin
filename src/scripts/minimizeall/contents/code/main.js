/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Thomas Lübking <thomas.luebking@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

    // Try to preserve last active window by sorting windows.
    relevantClients.sort((a, b) => {
        if (a.active) {
            return 1;
        } else if (b.active) {
            return -1;
        }
        return a.stackingOrder - b.stackingOrder;
    });

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
