/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 1.1

Item {
    property int paddingLeft
    property int paddingRight
    property int paddingTop
    property int paddingBottom
    property int borderLeft
    property int borderRight
    property int borderTop
    property int borderBottom
    property int borderLeftMaximized
    property int borderRightMaximized
    property int borderTopMaximized
    property int borderBottomMaximized
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: decoration.titleMouseMoved(mouse.button, mouse.buttons)
        onPressed: decoration.titlePressed(mouse.button, mouse.buttons)
        onReleased: decoration.titleReleased(mouse.button, mouse.buttons)
    }
}
