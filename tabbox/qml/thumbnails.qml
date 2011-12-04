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
import org.kde.kwin 0.1 as KWin

Item {
    id: thumbnailTabBox
    property int screenWidth : 1
    property int screenHeight : 1
    property real screenFactor: screenWidth/screenHeight
    property int imagePathPrefix: (new Date()).getTime()
    property int optimalWidth: (thumbnailListView.thumbnailWidth + hoverItem.margins.left + hoverItem.margins.right) * thumbnailListView.count + background.margins.left + background.margins.bottom
    property int optimalHeight: thumbnailListView.thumbnailWidth*(1.0/screenFactor) + hoverItem.margins.top + hoverItem.margins.bottom + background.margins.top + background.margins.bottom + 40
    property bool canStretchX: false
    property bool canStretchY: false
    width: Math.min(Math.max(screenWidth * 0.3, optimalWidth), screenWidth * 0.9)
    height: Math.min(Math.max(screenHeight * 0.15, optimalHeight), screenHeight * 0.7)
    clip: true


    function setModel(model) {
        thumbnailListView.model = model;
        thumbnailListView.imageId++;
    }

    function modelChanged() {
        thumbnailListView.imageId++;
    }

    PlasmaCore.FrameSvgItem {
        id: background
        anchors.fill: parent
        imagePath: "dialogs/background"
    }
    // just to get the margin sizes
    PlasmaCore.FrameSvgItem {
        id: hoverItem
        imagePath: "widgets/viewitem"
        prefix: "hover"
        visible: false
    }

    PlasmaCore.Theme {
        id: theme
    }

    ListView {
        /**
         * Called from C++ to get the index at a mouse pos.
         **/
        function indexAtMousePos(pos) {
            return thumbnailListView.indexAt(pos.x, pos.y);
        }
        signal currentIndexChanged(int index)
        id: thumbnailListView
        objectName: "listView"
        orientation: ListView.Horizontal
        // used for image provider URL to trick Qt into reloading icons when the model changes
        property int imageId: 0
        property int thumbnailWidth: 300
        height: thumbnailWidth * (1.0/screenFactor) + hoverItem.margins.bottom + hoverItem.margins.top
        spacing: 5
        highlightMoveDuration: 250
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: background.margins.top
            leftMargin: background.margins.left
            rightMargin: background.margins.right
            bottomMargin: background.margins.bottom
        }
        clip: true
        delegate: Item {
            property alias data: thumbnailItem.data
            id: delegateItem
            width: thumbnailListView.thumbnailWidth
            height: thumbnailListView.thumbnailWidth*(1.0/screenFactor)
            KWin.ThumbnailItem {
                property variant data: model
                id: thumbnailItem
                wId: windowId
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
                    thumbnailListView.currentIndex = index;
                    thumbnailListView.currentIndexChanged(thumbnailListView.currentIndex);
                }
            }
        }
        highlight: PlasmaCore.FrameSvgItem {
            id: highlightItem
            imagePath: "widgets/viewitem"
            prefix: "hover"
            width: thumbnailListView.thumbnailWidth
            height: thumbnailListView.thumbnailWidth*(1.0/screenFactor)
        }
    }
    Item {
        height: 40
        anchors {
            top: thumbnailListView.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            topMargin: hoverItem.margins.bottom
            leftMargin: background.margins.left
            rightMargin: background.margins.right
            bottomMargin: background.margins.bottom
        }
        Image {
            id: iconItem
            source: "image://client/" + thumbnailListView.currentIndex + "/" + thumbnailTabBox.imagePathPrefix + "-" + thumbnailListView.imageId
            width: 32
            height: 32
            sourceSize {
                width: 32
                height: 32
            }
            anchors {
                verticalCenter: parent.verticalCenter
                right: textItem.left
                rightMargin: 4
            }
        }
        Text {
            id: textItem
            text: thumbnailListView.currentItem ? thumbnailListView.currentItem.data.caption : ""
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: theme.textColor
            font {
                bold: true
            }
            anchors {
                verticalCenter: parent.verticalCenter
                horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
