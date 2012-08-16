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

Item {
    id : preview
    Loader {
        property int screenWidth : preview.width
        property int screenHeight : preview.height
        property bool allDesktops: true
        width: preview.width
        height: preview.height - textElement.height
        source: sourcePath
        anchors.centerIn: parent
        onLoaded: {
            if (item.allDesktops != undefined) {
                item.allDesktops = allDesktops;
            }
            if (item.setModel) {
                item.setModel(clientModel);
            }
            if (item.screenWidth != undefined) {
                item.screenWidth = screenWidth;
            }
            if (item.screenHeight != undefined) {
                item.screenHeight = screenHeight;
            }
            item.width = preview.width;
            item.height = preview.height - textElement.height;
        }
    }
    Text {
        id: textElement
        font.bold: true
        text: name
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
        }
        visible: true
    }
}
