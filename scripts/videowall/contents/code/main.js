/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

var applyTo = readConfig("ApplyTo", true);
var whitelist = readConfig("Whitelist", "vlc, xv, vdpau, smplayer, dragon, xine, ffplay, mplayer").toString().toLowerCase().split(",");
for (i = 0; i < whitelist.length; ++i)
    whitelist[i] = whitelist[i].trim();

var ignore = readConfig("Ignore", false);
var blacklist = readConfig("Blacklist", "").toString().toLowerCase().split(",");
for (i = 0; i < blacklist.length; ++i)
    blacklist[i] = blacklist[i].trim();


function isVideoPlayer(client) {
    if (applyTo == true && whitelist.indexOf(client.resourceClass.toString()) < 0)
        return false; // required whitelist match failed
    if (ignore == true && blacklist.indexOf(client.resourceClass.toString()) > -1)
        return false; // required blacklist match hit
    return true;
}

var videowall = function(client, set) {
    if (set && isVideoPlayer(client)) {
        client.frameGeometry = workspace.clientArea(KWin.FullArea, 0, 1);
    }
};

workspace.clientFullScreenSet.connect(videowall);
