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
    id: iconsTabBox
    property int iconSize
    property int imagePathPrefix: (new Date()).getTime()
    property alias count: iconsListView.count
    property alias margins: hoverItem.margins
    property alias currentItem: iconsListView.currentItem
    focus: true


    function setModel(model) {
        iconsListView.model = model;
        iconsListView.imageId++;
    }

    function modelChanged() {
        iconsListView.imageId++;
    }

    PlasmaCore.Theme {
        id: theme
    }

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
            property alias data: iconItem.data
            id: delegateItem
            width: iconSize + hoverItem.margins.left + hoverItem.margins.right
            height: iconSize + hoverItem.margins.top + hoverItem.margins.bottom
            Image {
                property variant data: model
                id: iconItem
                source: "image://client/" + index + "/" + iconsTabBox.imagePathPrefix + "-" + iconsListView.imageId + (index == iconsListView.currentIndex ? "/selected" : "")
                sourceSize {
                    width: iconSize
                    height: iconSize
                }
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
                    iconsListView.currentIndexChanged(iconsListView.currentIndex);
                }
            }
        }
    }
    ListView {
        signal currentIndexChanged(int index)
        id: iconsListView
        objectName: "listView"
        orientation: ListView.Horizontal
        // used for image provider URL to trick Qt into reloading icons when the model changes
        property int imageId: 0
        anchors {
            fill: parent
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
    }
    /*
     * Key navigation on outer item for two reasons:
     * @li we have to emit the change signal
     * @li on multiple invocation it does not work on the list view. Focus seems to be lost.
     **/
    Keys.onPressed: {
        if (event.key == Qt.Key_Left) {
            iconsListView.decrementCurrentIndex();
            iconsListView.currentIndexChanged(iconsListView.currentIndex);
        } else if (event.key == Qt.Key_Right) {
            iconsListView.incrementCurrentIndex();
            iconsListView.currentIndexChanged(iconsListView.currentIndex);
        }
    }
}
