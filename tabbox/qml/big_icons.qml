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
import org.kde.plasma.core 0.1 as PlasmaCore
import org.kde.qtextracomponents 0.1

Item {
    id: bigIconsTabBox
    property int screenWidth : 0
    property int screenHeight : 0
    property int imagePathPrefix: (new Date()).getTime()
    property int optimalWidth: (icons.iconSize + icons.margins.left + icons.margins.right) * icons.count + background.margins.left + background.margins.bottom
    property int optimalHeight: icons.iconSize + icons.margins.top + icons.margins.bottom + background.margins.top + background.margins.bottom
    property bool canStretchX: false
    property bool canStretchY: false
    width: Math.min(Math.max(screenWidth * 0.3, optimalWidth), screenWidth * 0.9)
    height: Math.min(Math.max(screenHeight * 0.05, optimalHeight), screenHeight * 0.5)


    function setModel(model) {
        icons.setModel(model);
    }

    function modelChanged() {
        icons.modelChanged();
    }

    PlasmaCore.FrameSvgItem {
        id: background
        anchors.fill: parent
        imagePath: "dialogs/background"
    }

    IconTabBox {
        id: icons
        iconSize: 64
        anchors {
            fill: parent
            topMargin: background.margins.top
            rightMargin: background.margins.right
            bottomMargin: background.margins.bottom
            leftMargin: background.margins.left
        }
    }
}
