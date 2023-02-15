/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Thomas Lübking <thomas.luebking@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

var registeredBorders = [];

function isRelevant(window) {
    return window.minimizable &&
           (!window.desktops.length || window.desktops.indexOf(workspace.currentDesktop) != -1);
           (!window.activities.length || window.activities.indexOf(workspace.currentActivity.toString()) > -1);
}

function minimizeAllWindows() {
    var allWindows = workspace.windowList();
    var relevantWindows = [];
    var minimize = false;

    for (var i = 0; i < allWindows.length; ++i) {
        if (!isRelevant(allWindows[i])) {
            continue;
        }
        if (!allWindows[i].minimized) {
            minimize = true;
        }
        relevantWindows.push(allWindows[i]);
    }

    // Try to preserve last active window by sorting windows.
    relevantWindows.sort((a, b) => {
        if (a.active) {
            return 1;
        } else if (b.active) {
            return -1;
        }
        return a.stackingOrder - b.stackingOrder;
    });

    for (var i = 0; i < relevantWindows.length; ++i) {
        var wasMinimizedByScript = relevantWindows[i].minimizedByScript;
        delete relevantWindows[i].minimizedByScript;

        if (minimize) {
            if (relevantWindows[i].minimized) {
                continue;
            }
            relevantWindows[i].minimized = true;
            relevantWindows[i].minimizedByScript = true;
        } else {
            if (!wasMinimizedByScript) {
                continue;
            }
            relevantWindows[i].minimized = false;
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
