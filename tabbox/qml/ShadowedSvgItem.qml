/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Weng Xuetian <wengxt@gmail.com>

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

Item {
    property double leftMargin: shadow.margins.left + background.margins.left
    property double topMargin: shadow.margins.top + background.margins.top
    property double rightMargin: shadow.margins.right + background.margins.right
    property double bottomMargin: shadow.margins.bottom + background.margins.bottom
    property double centerWidth: shadow.width - shadow.margins.left - shadow.margins.right
    property double centerHeight: shadow.height - shadow.margins.bottom - shadow.margins.top
    property int centerTopMargin: shadow.margins.top
    property int centerLeftMargin: shadow.margins.left
    property alias maskImagePath: shadow.imagePath

    PlasmaCore.FrameSvg {
        id: themeInfo
        imagePath: "dialogs/background"
        property bool hasNewShadows: hasElementPrefix("shadow")
    }

    PlasmaCore.FrameSvgItem {
        id: shadow
        prefix: themeInfo.hasNewShadows ? "shadow" : ""

        imagePath: "dialogs/background"
        anchors.fill: parent
        visible: true

        PlasmaCore.FrameSvgItem {
            id: background
            imagePath: shadow.imagePath
            visible: themeInfo.hasNewShadows
            anchors {
                fill: parent
                leftMargin: shadow.margins.left
                topMargin: shadow.margins.top
                rightMargin: shadow.margins.right
                bottomMargin: shadow.margins.bottom
            }
        }
    }
}
