/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Thomas Lübking <thomas.luebking@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

var registeredBorders = [];

function isRelevant(window) {
    return window.minimizable &&
        (!window.desktops.length || window.desktops.includes(workspace.currentDesktop)) &&
        (!window.activities.length || window.activities.includes(workspace.currentActivity));
}

function minimizeWindows(windows) {
    let minimize = false;
    var relevantWindows = [];
    for (const window of windows) {
        if (!isRelevant(window)) {
            continue;
        }
        if (!window.minimized) {
            minimize = true;
        }
        relevantWindows.push(window);
    }
    // Try to preserve topmost window by sorting windows.
    relevantWindows.sort((a, b) => {
        if (a.active) {
            return 1;
        } else if (b.active) {
            return -1;
        }
        return a.stackingOrder - b.stackingOrder;
    });

    for (const window of relevantWindows) {
        var wasMinimizedByScript = window.minimizedByScript;
        delete window.minimizedByScript;

        if (minimize) {
            if (window.minimized) {
                continue;
            }
            window.minimized = true;
            window.minimizedByScript = true;
        } else {
            if (!wasMinimizedByScript) {
                continue;
            }
            window.minimized = false;
        }
    }
    return [relevantWindows, minimize];
}

function minimizeWindowsRestoreFocus(windows) {
    const [relevantWindows, minimize] = minimizeWindows(windows);
    if (!minimize) {
        for (let i = relevantWindows.length - 1; i >= 0; i--) {
            const window = relevantWindows[i];
            if (window === workspace.activeWindow) {
                break;
            }
            if (!workspace.windowList().includes(window)) {
                continue;
            }
            workspace.activeWindow = window;
            break;
        }
    }
}

function minimizeAllWindows() {
    minimizeWindowsRestoreFocus(workspace.windowList());
}

function minimizeAllWindowsActiveScreen() {
    minimizeWindowsRestoreFocus(workspace.windowList().filter(window => window.output === workspace.activeScreen));
}

function minimizeAllOthers() {
    minimizeWindows(workspace.windowList().filter(window => !window.active));
}

function minimizeAllOthersActiveScreen() {
    minimizeWindows(workspace.windowList().filter(window => !window.active && window.output === workspace.activeScreen));
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

registerShortcut("MinimizeAll", "Minimize all windows", "Meta+Shift+D", minimizeAllWindows);
registerShortcut("MinimizeAllActiveScreen", "Minimize all windows in active screen", "", minimizeAllWindowsActiveScreen);
registerShortcut("minimizeAllOthers", "Minimize all other windows", "Meta+Shift+O", minimizeAllOthers);
registerShortcut("minimizeAllOthersActiveScreen", "Minimize all other windows in active screen", "", minimizeAllOthersActiveScreen);
init();
