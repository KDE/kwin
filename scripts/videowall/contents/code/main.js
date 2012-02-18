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
function isVideoPlayer(client) {
    if (client.resourceName == "vlc") {
        return true;
    }
    if (client.resourceName == "smplayer") {
        return true;
    }
    if (client.resourceName == "dragon") {
        return true;
    }
    if (client.resourceName == "xv") { //mplayer
        return true;
    }
    if (client.resourceName == "ffplay") {
        return true;
    }
    return false;
}

var videowall = function(client, set) {
    if (isVideoPlayer(client) && set) {
        client.geometry = workspace.clientArea(KWin.FullArea, 0, 1);
    }
};

workspace.clientFullScreenSet.connect(videowall);
