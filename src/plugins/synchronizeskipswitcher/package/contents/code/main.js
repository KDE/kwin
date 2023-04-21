/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
function synchronizeSwitcher(client) {
    client.skipSwitcher = client.skipTaskbar;
}

function setupConnection(client) {
    synchronizeSwitcher(client);
    client.skipTaskbarChanged.connect(client, synchronizeSwitcher);
}

workspace.clientAdded.connect(setupConnection);
// connect all existing clients
var clients = workspace.clientList();
for (var i=0; i<clients.length; i++) {
    setupConnection(clients[i]);
}
