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
import QtQuick 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kquickcontrolsaddons 2.0

Item {
    id: iconsTabBox
    function decrementCurrentIndex() {
        iconsListView.decrementCurrentIndex();
    }
    function incrementCurrentIndex() {
        iconsListView.incrementCurrentIndex();
    }
    property int iconSize
    property alias count: iconsListView.count
    property alias margins: hoverItem.margins
    property alias currentItem: iconsListView.currentItem
    property alias model: iconsListView.model
    property alias currentIndex: iconsListView.currentIndex
    focus: true
    clip: true

    // just to get the margin sizes
    PlasmaCore.FrameSvgItem {
        id: hoverItem
        imagePath: "widgets/viewitem"
        prefix: "hover"
        visible: false
    }

    // delegate
    Component {
        id: listDelegate
        Item {
            property alias caption: iconItem.caption
            id: delegateItem
            width: iconSize + hoverItem.margins.left + hoverItem.margins.right
            height: iconSize + hoverItem.margins.top + hoverItem.margins.bottom
            QIconItem {
                property variant caption: model.caption
                id: iconItem
                icon: model.icon
                width: iconSize
                height: iconSize
                state: index == iconsListView.currentIndex ? QIconItem.ActiveState : QIconItem.DefaultState
                anchors {
                    fill: parent
                    leftMargin: hoverItem.margins.left
                    rightMargin: hoverItem.margins.right
                    topMargin: hoverItem.margins.top
                    bottomMargin: hoverItem.margins.bottom
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    iconsListView.currentIndex = index;
                }
            }
        }
    }
    ListView {
        id: iconsListView
        orientation: ListView.Horizontal
        width: Math.min(parent.width, (iconSize + margins.left + margins.right) * count)
        height: iconSize + margins.top + margins.bottom
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
        }
        clip: true
        delegate: listDelegate
        highlight: PlasmaCore.FrameSvgItem {
            id: highlightItem
            imagePath: "widgets/viewitem"
            prefix: "hover"
            width: iconSize + margins.left + margins.right
            height: iconSize + margins.top + margins.bottom
        }
        highlightMoveDuration: 250
        boundsBehavior: Flickable.StopAtBounds
    }
}
