/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
function synchronizeSwitcher(window) {
    window.skipSwitcher = window.skipTaskbar;
}

function setupConnection(window) {
    synchronizeSwitcher(window);
    window.skipTaskbarChanged.connect(window, synchronizeSwitcher);
}

workspace.windowAdded.connect(setupConnection);
// connect all existing windows
var windows = workspace.windowList();
for (var i=0; i<windows.length; i++) {
    setupConnection(windows[i]);
}
