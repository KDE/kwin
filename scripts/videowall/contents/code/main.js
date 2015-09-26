/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
        client.geometry = workspace.clientArea(KWin.FullArea, 0, 1);
    }
};

workspace.clientFullScreenSet.connect(videowall);
