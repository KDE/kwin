/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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
import QtQuick 1.0

Loader {
    id: loader
    focus: true
    property int screenWidth : 0
    property int screenHeight : 0
    property bool allDesktops: true
    property string longestCaption: ""
    onLoaded: {
        if (item.screenWidth != undefined) {
            item.screenWidth = screenWidth;
        }
        if (item.screenHeight != undefined) {
            item.screenHeight = screenHeight;
        }
        if (item.allDesktops != undefined) {
            item.allDesktops = allDesktops;
        }
        if (item.longestCaption != undefined) {
            item.longestCaption = longestCaption;
        }
        if (item.setModel) {
            item.setModel(clientModel);
        }
    }
    onScreenWidthChanged: {
        if (item && item.screenWidth != undefined) {
            item.screenWidth = screenWidth;
        }
    }
    onScreenHeightChanged: {
        if (item && item.screenHeight != undefined) {
            item.screenHeight = screenHeight;
        }
    }
    onAllDesktopsChanged: {
        if (item && item.allDesktops != undefined) {
            item.allDesktops= allDesktops;
        }
    }
    onLongestCaptionChanged: {
        if (item && item.longestCaption != undefined) {
            item.longestCaption = longestCaption;
        }
    }
    Connections {
        target: clientModel
        onModelReset: {
            if (item && item.modelChanged) {
                item.modelChanged();
            }
        }
    }
}
