/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
import QtQuick 1.1
import org.kde.plasma.core 0.1 as PlasmaCore
import org.kde.qtextracomponents 0.1
import org.kde.kwin 0.1 as KWin

Item {
    id: presentWindowsTabBox
    property int screenWidth : 1
    property int screenHeight : 1
    property int optimalWidth: 0.9*screenWidth
    property int optimalHeight: 0.9*screenHeight
    property int imagePathPrefix: (new Date()).getTime()
    property int standardMargin: 2
    width: optimalWidth
    height: optimalHeight
    focus: true

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

    GridView {
        signal currentIndexChanged(int index)
        // used for image provider URL to trick Qt into reloading icons when the model changes
        property int imageId: 0
        property int rows: Math.round(Math.sqrt(count))
        property int columns: (rows*rows < count) ? rows + 1 : rows
        id: thumbnailListView
        objectName: "listView"
        cellWidth: Math.floor(width / columns)
        cellHeight: Math.floor(height / rows)
        anchors {
            fill: parent
            leftMargin: background.margins.left
            rightMargin: background.margins.right
            topMargin: background.margins.top
            bottomMargin: background.margins.bottom
        }
        delegate: Item {
            width: thumbnailListView.cellWidth
            height: thumbnailListView.cellHeight
            KWin.ThumbnailItem {
                id: thumbnailItem
                wId: windowId
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    bottom: captionItem.top
                    leftMargin: hoverItem.margins.left
                    rightMargin: hoverItem.margins.right
                    topMargin: hoverItem.margins.top
                    bottomMargin: standardMargin
                }
            }
            Item {
                id: captionItem
                height: childrenRect.height
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    leftMargin: hoverItem.margins.left + standardMargin
                    bottomMargin: hoverItem.margins.bottom
                    rightMargin: hoverItem.margins.right
                }
                Image {
                    id: iconItem
                    source: "image://client/" + index + "/" + presentWindowsTabBox.imagePathPrefix + "-" + thumbnailListView.imageId
                    width: 32
                    height: 32
                    sourceSize {
                        width: 32
                        height: 32
                    }
                    anchors {
                        bottom: parent.bottom
                        right: textItem.left
                    }
                }
                Item {
                    id: textItem
                    property int maxWidth: parent.width - iconItem.width - parent.anchors.leftMargin - parent.anchors.rightMargin - anchors.leftMargin - standardMargin * 2
                    width: (textElementSelected.implicitWidth >= maxWidth) ? maxWidth : textElementSelected.implicitWidth
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        horizontalCenter: parent.horizontalCenter
                        leftMargin: standardMargin
                    }
                    Text {
                        id: textElementSelected
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: caption
                        font.italic: minimized
                        font.bold: true
                        visible: index == thumbnailListView.currentIndex
                        color: theme.textColor
                        elide: Text.ElideMiddle
                        anchors.fill: parent
                    }
                    Text {
                        id: textElementNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: caption
                        font.italic: minimized
                        visible: index != thumbnailListView.currentIndex
                        color: theme.textColor
                        elide: Text.ElideMiddle
                        anchors.fill: parent
                    }
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
            width: thumbnailListView.cellWidth
            height: thumbnailListView.cellHeight
        }
    }
    /*
     * Key navigation on outer item for two reasons:
     * @li we have to emit the change signal
     * @li on multiple invocation it does not work on the list view. Focus seems to be lost.
     **/
    Keys.onPressed: {
        if (event.key == Qt.Key_Left) {
            thumbnailListView.moveCurrentIndexLeft();
            thumbnailListView.currentIndexChanged(thumbnailListView.currentIndex);
        } else if (event.key == Qt.Key_Right) {
            thumbnailListView.moveCurrentIndexRight();
            thumbnailListView.currentIndexChanged(thumbnailListView.currentIndex);
        } else if (event.key == Qt.Key_Up) {
            thumbnailListView.moveCurrentIndexUp();
            thumbnailListView.currentIndexChanged(thumbnailListView.currentIndex);
        } else if (event.key == Qt.Key_Down) {
            thumbnailListView.moveCurrentIndexDown();
            thumbnailListView.currentIndexChanged(thumbnailListView.currentIndex);
        }
    }
}
