/********************************************************************
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
import QtQuick 1.0

Item {
    property variant list
    property int itemHeight: 1
    id: verticalScroolbar
    width:19
    height: parent.height-1
    anchors.right: parent.right
    anchors.rightMargin: 2
    clip:true

    Item {
        id: scroolerArea
        width: parent.width
        height: parent.height-uparrow.height*2
        y:uparrow.height
        opacity: scroolerAreaOver.containsMouse? 1:list.moving?1:dragarea.containsMouse? 1:dragarea.pressed?1: 0.3
        Behavior on opacity {
            NumberAnimation {  duration: 600 }
        }
        MouseArea {
            id: scroolerAreaOver
            anchors.bottomMargin:-uparrow.height
            anchors.topMargin: -uparrow.height
            anchors.fill: parent
            hoverEnabled: true

        }
        BorderImage {
            id: scroller
            source: "images/scrool.png"
            width:parent.width
            height:Math.max (14,parent.height*list.height/list.contentHeight)
            y:parent.height*list.contentY/list.contentHeight
            border.left: 7; border.top: 7
            border.right: 7; border.bottom: 8
            opacity: 1
        }

        BorderImage {
            id: scrolleractive
            source: "images/scroolactive.png"
            width:parent.width
            height:Math.max (14,parent.height*list.height/list.contentHeight)
            y:parent.height*list.contentY/list.contentHeight
            border.left: 7; border.top: 7
            border.right: 7; border.bottom: 8
            opacity:scroolerAreaOver.containsMouse? 1:dragarea.containsMouse?1:dragarea.pressed?1:0.01
            Behavior on opacity {
                NumberAnimation {  duration: 400 }
            }
            MouseArea{
                id:dragarea
                anchors.fill:parent
                hoverEnabled: true
                drag.target: scrolleractive
                drag.axis: Drag.YAxis
                drag.minimumY: 0
                drag.maximumY: scroolerArea.height-scrolleractive.height
                onPositionChanged: list.contentY=scrolleractive.y*list.contentHeight/(scroolerArea.height)
            }
        }
    }

    Item{
        id:uparrow
        width:parent.width
        height: parent.width-4
        Image{
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            source:"images/arrowup.png"
            opacity:uparrowOver.containsMouse? 0:1

            Behavior on opacity {
                NumberAnimation {  duration: 300 }
            }
        }
        Image{
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            source:"images/arrowupactive.png"
            opacity: uparrowOver.containsMouse? 1:0
            Behavior on opacity {
                NumberAnimation {  duration: 300 }
            }
        }
        MouseArea {
            id: uparrowOver
            anchors.fill: parent
            hoverEnabled: true
            onClicked: list.contentY -= itemHeight
        }
    }

    Item{
        id:downarrow
        width:parent.width
        height: parent.width-4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        Image{
            rotation: 180
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            source:"images/arrowup.png"
            opacity:downarrowOver.containsMouse? 0:1

            Behavior on opacity {
                NumberAnimation {  duration: 300 }
            }
        }
        Image{
            rotation: 180
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            source:"images/arrowupactive.png"
            opacity: downarrowOver.containsMouse? 1:0
            Behavior on opacity {
                NumberAnimation {  duration: 300 }
            }
        }
        MouseArea {
            id: downarrowOver
            anchors.fill: parent
            hoverEnabled: true
            onClicked: list.contentY += itemHeight
        }
    }
}
