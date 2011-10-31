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
import org.kde.plasma.components 0.1 as PlasmaComponents
Item {
    id: container
    GridView {
        property int lastMousePosX
        property int lastMousePosY
        id: view
        objectName: "view"
        model: layoutModel
        cellWidth: Math.max(100, (container.width % 2 == 0 ? container.width : (container.width - 1)) / 2.0)
        cellHeight: Math.max(100, container.height / (count % 2 == 0 ? count : (count+1)) * 2)
        anchors.fill: parent
        delegate: itemDelegate
        highlight: PlasmaComponents.Highlight {
            width: view.cellWidth
            height: view.cellHeight
            hover: true
        }
        MouseArea {
            hoverEnabled: true
            anchors.fill: parent
            onPositionChanged: {
                view.lastMousePosX = mouse.x;
                view.lastMousePosY = mouse.y;
            }
            onClicked: {
                view.currentIndex = view.indexAt(mouse.x, mouse.y);
            }
        }
    }
    Component {
        id: itemDelegate
        Item {
            width: view.cellWidth
            height: view.cellHeight
            Loader {
                property int screenWidth : container.width
                property int screenHeight : container.height
                property bool allDesktops: true
                width: {
                    if (item.canStretchX) {
                        return Math.min(Math.max(item.optimalWidth, view.cellWidth), view.cellWidth)
                    } else {
                        return Math.min(item.optimalWidth, view.cellWidth);
                    }
                }
                height: Math.min(item.optimalHeight, view.cellHeight)
                source: sourcePath
                anchors.centerIn: parent
                onLoaded: {
                    if (item.allDesktops != undefined) {
                        item.allDesktops = allDesktops;
                    }
                    if (item.setModel) {
                        item.setModel(clientModel);
                    }
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
                visible: view.indexAt(view.lastMousePosX, view.lastMousePosY) == index
            }
        }
    }
}
