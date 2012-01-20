/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2012 Nuno Pinheiro <nuno@oxygen-icons.org>

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
import org.kde.qtextracomponents 0.1 as QtExtra

BorderImage {
    property alias currentIndex: listView.currentIndex
    source: "images/bg.png"
    border {
        left: 4
        top: 4
        right: 4
        bottom: 4
    }

    MouseArea {
        id: focusOver
        anchors.fill: parent
        hoverEnabled: true
    }

    ListView {
        id: listView
        x: 3
        height: parent.height - 1 // -1 on the account of the last pixel being a glow pixel
        width: parent.width - 6 - scrollbar.width
        model: decorationModel
        highlight: Rectangle {
            width: listView.width
            height: 150
            color: "lightsteelblue"
            opacity: 0.5
        }
        delegate: Item {
            width: listView.width
            height: 150
            QtExtra.QPixmapItem {
                pixmap: preview
                anchors.fill: parent
                visible: type == 0
            }
            Loader {
                source: type == 1 ? "AuroraePreview.qml" : ""
                anchors.fill: parent
            }
            MouseArea {
                hoverEnabled: false
                anchors.fill: parent
                onClicked: {
                    listView.currentIndex = index;
                }
            }
        }
    }
    OxygenScrollbar {
        id: scrollbar
        list: listView
        itemHeight: 150
    }

    BorderImage {
        id: shadow
        source: "images/shadow.png"
        anchors.fill:parent
        border.left: 4; border.top: 4
        border.right: 4; border.bottom: 5
        opacity:focusOver.containsMouse? 0.01:1 //insert here a proper focus mechanism
        Behavior on opacity {
            NumberAnimation {  duration: 300 }
        }

    }

    BorderImage {
        id: glow
        source: "images/glow.png"
        anchors.fill:parent
        border.left: 4; border.top: 4
        border.right: 4; border.bottom: 5
        opacity:focusOver.containsMouse? 1:0 //insert here a proper focus mechanism
        Behavior on opacity {
            NumberAnimation {  duration: 300 }
        }
    }
}
