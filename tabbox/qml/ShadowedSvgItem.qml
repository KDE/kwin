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
    property double leftMargin: background.margins.left + shadow.margins.left
    property double topMargin: background.margins.top + shadow.margins.top
    property double rightMargin: background.margins.right + shadow.margins.right
    property double bottomMargin: background.margins.bottom + shadow.margins.bottom
    property double centerWidth: background.width
    property double centerHeight: background.height
    property int centerTopMargin: shadow.margins.top
    property int centerLeftMargin: shadow.margins.left

    PlasmaCore.FrameSvgItem {
        id: shadow
        imagePath: "dialogs/background"
        prefix: "shadow"
        anchors.fill: parent

        PlasmaCore.FrameSvgItem {
            id: background
            imagePath: "dialogs/background"
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
