/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

let ignore = readConfig("Ignore", false);
let ignoreList = ["plasmashell", "krunner"];
if (ignore) {
    ignoreList.concat(readConfig("IgnoreList", "vlc, xv, vdpau, smplayer, dragon, xine, ffplay, mplayer").toString().toLowerCase().split(","));
}

for (i = 0; i < ignoreList.length; ++i) {
    ignoreList[i] = ignoreList[i].trim();
}

function ignoreClient(client) {
    if (client.minimized || client.transient) {
        return true;
    } else if (ignoreList.indexOf(client.resourceClass.toString()) >= 0) {
        return true;
    } else {
        return false;
    }
}

function searchEmptyTile(screenIdx) {
    const tiling = workspace.tilingForScreen(screenIdx);

    if (tiling.rootTile.tiles.length === 0) {
        if (!("windows" in tiling.rootTile) || tiling.rootTile.windows.length === 0) {
            return tiling.rootTile;
        } else {
            return null;
        }
    }

    let stack = [tiling.rootTile];

    while (stack.length > 0) {
        let candidate = stack[0];
        stack.splice(0, 1);

        if (!candidate.isLayout) {
            if (!("windows" in candidate) || candidate.windows.length === 0) {
                return candidate;
            }
        } else {
            stack.concat(candidate.tiles);
        }
    }

    return null;
}

function lastTile(screenIdx) {
    const tiling = workspace.tilingForScreen(screenIdx);
    let stack = [tiling.rootTile];
    let candidate = tiling.rootTile;

    while (stack.length > 0) {
        candidate = stack[0];
        stack.splice(0, 1);

        if (candidate.isLayout) {
            for (let i in candidate.tiles) {
                stack.push(candidate.tiles[i]);
            }
        }
    }
    return candidate;
}

function assignTileToWindow(tile, client) {
    if (!("windows" in tile) || tile.windows.length == 0) {
        client.tile = tile;
        return;
    }

    // Fail when tiles get too small
    if (tile.absoluteGeometry.width < 200 && tile.absoluteGeometry.height < 200) {
        return;
    }
    const tiling = workspace.tilingForScreen(client.screen);

    if (tile === tiling.rootTile || tile.parent.layoutDirection === 2) {
        tile.split(1);
    } else {
        tile.split(2);
    }

    if (!("windows" in tile.tiles[0])) {
        tile.tiles[0].windows = [];
    }
    if (!("windows" in tile.tiles[1])) {
        tile.tiles[1].windows = [];
    }

    if (tile.tiles[0].windows.length == 0) {
        client.tile = tile.tiles[0];
    } else {
        client.tile = tile.tiles[1];
    }
}

var trackTileToWindow = function(client) {
    var trackTileToWindowImpl = function(tile) {
        if (tile) {
            if (!("windows" in tile)) {
                tile.windows = [];
            }

            tile.windows.push(client);

            const tiling = workspace.tilingForScreen(client.screen);
            tiling.tileRemoved.connect((tile) => {
                if (client.oldTile === tile) {
                    client.oldTile = null;
                }
            });
        }

        if (("oldTile" in client) && client.oldTile && ("windows" in client.oldTile)) {
            let idx = client.oldTile.windows.indexOf(client);
            if (idx >= 0) {
                client.oldTile.windows.splice(idx, 1);
                if (!client.oldTile.isLayout && client.oldTile.windows.length === 0) {
                    client.oldTile.remove();
                }
            }
        }
        client.oldTile = tile;
    };
    return trackTileToWindowImpl;
};

var clientAdded = function(client) {

    if (ignoreClient(client)) {
        return;
    }

    if (!("interactiveMoveResizeTracker" in client)) {
        client.moveResizedChanged.connect(() => {
            if (!client.tile && !client.move) {
                assignTileToWindow(tiling.bestTileForPosition(client.frameGeometry.x, client.frameGeometry.y), client);
            }
        });
        client.interactiveMoveResizeTracker = true;
    }

    const tiling = workspace.tilingForScreen(client.screen);

    client.tileChanged.connect(trackTileToWindow(client));

    let tile = searchEmptyTile(client.screen);

    if (!tile) {
        tile = lastTile(client.screen);
    }

    assignTileToWindow(tile, client);
};

var clientRemoved = function(client) {
    const tile = client.tile;
    if (!tile || !("windows" in tile)) {
        return;
    }
    let idx = tile.windows.indexOf(client);

    if (idx >= 0) {
        tile.windows.splice(idx, 1);
    }

    if (tile.windows.length === 0) {
        tile.remove();
    }
};

workspace.clientAdded.connect(clientAdded);
workspace.clientRemoved.connect(clientRemoved);
workspace.clientMinimized.connect(clientRemoved);
workspace.clientUnminimized.connect((client) => {
    if (!client.minimized) {
        const tiling = workspace.tilingForScreen(client.screen);
        assignTileToWindow(tiling.bestTileForPosition(client.frameGeometry.x, client.frameGeometry.y), client);
    }
});

let clients = workspace.clientList();
for (let i = 0; i < clients.length; i++) {
    clientAdded(clients[i]);
}
