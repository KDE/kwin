/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

function setupConnection(window) {
    window.skipSwitcher = window.skipTaskbar;
    window.skipTaskbarChanged.connect(() => {
        window.skipSwitcher = window.skipTaskbar;
    });
}

workspace.windowAdded.connect(setupConnection);
// connect all existing clients
var clients = workspace.windowList();
for (var i=0; i<clients.length; i++) {
    setupConnection(clients[i]);
}
