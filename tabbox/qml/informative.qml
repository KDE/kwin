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
    id: informativeTabBox
    property int screenWidth : 0
    property int screenHeight : 0
    property bool allDesktops: true
    property string longestCaption: ""
    property int imagePathPrefix: (new Date()).getTime()
    property int optimalWidth: listView.maxRowWidth
    property int optimalHeight: listView.rowHeight * listView.count + background.margins.top + background.margins.bottom
    property bool canStretchX: true
    property bool canStretchY: false
    width: Math.min(Math.max(screenWidth * 0.2, optimalWidth), screenWidth * 0.8)
    height: Math.min(Math.max(screenHeight * 0.2, optimalHeight), screenHeight * 0.8)

    property int textMargin: 2

    onLongestCaptionChanged: {
        listView.maxRowWidth = listView.calculateMaxRowWidth();
    }

    function setModel(model) {
        listView.model = model;
        listView.maxRowWidth = listView.calculateMaxRowWidth();
        listView.imageId++;
    }

    function modelChanged() {
        listView.imageId++;
    }

    /**
     * Returns the caption with adjustments for minimized items.
     * @param caption the original caption
     * @param mimized whether the item is minimized
     * @return Caption adjusted for minimized state
     **/
    function itemCaption(caption, minimized) {
        var text = caption;
        if (minimized) {
            text = "(" + text + ")";
        }
        return text;
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

    PlasmaCore.FrameSvgItem {
        id: background
        anchors.fill: parent
        imagePath: "dialogs/background"
    }

    // delegate
    Component {
        id: listDelegate
        Item {
            id: delegateItem
            width: listView.width
            height: listView.rowHeight
            Image {
                id: iconItem
                source: "image://client/" + index + "/" + informativeTabBox.imagePathPrefix + "-" + listView.imageId + (index == listView.currentIndex ? "/selected" : "/disabled")
                width: 32
                height: 32
                sourceSize {
                    width: 32
                    height: 32
                }
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    leftMargin: hoverItem.margins.left
                }
            }
            Text {
                id: captionItem
                horizontalAlignment: Text.AlignHCenter
                text: itemCaption(caption, minimized)
                font.bold: true
                font.italic: minimized
                color: theme.textColor
                elide: Text.ElideMiddle
                anchors {
                    left: iconItem.right
                    right: parent.right
                    top: parent.top
                    topMargin: informativeTabBox.textMargin + hoverItem.margins.top
                    rightMargin: hoverItem.margins.right
                }
            }
            Text {
                id: desktopNameItem
                horizontalAlignment: Text.AlignHCenter
                text: desktopName
                font.bold: false
                font.italic: true
                color: theme.textColor
                elide: Text.ElideMiddle
                visible: informativeTabBox.allDesktops
                anchors {
                    left: iconItem.right
                    right: parent.right
                    top: captionItem.bottom
                    topMargin: informativeTabBox.textMargin
                    bottom: parent.bottom
                    bottomMargin: informativeTabBox.textMargin + hoverItem.margins.bottom
                    rightMargin: hoverItem.margins.right
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listView.currentIndex = index;
                    listView.currentIndexChanged(listView.currentIndex);
                }
            }
        }
    }
    ListView {
        function calculateMaxRowWidth() {
            var width = 0;
            var textElement = Qt.createQmlObject(
                'import Qt 4.7;'
                + 'Text {\n'
                + '     text: "' + itemCaption(informativeTabBox.longestCaption, true) + '"\n'
                + '     font.bold: true\n'
                + '     visible: false\n'
                + '}',
                listView, "calculateMaxRowWidth");
            width = Math.max(textElement.width, width);
            textElement.destroy();
            return width + 32 + hoverItem.margins.right + hoverItem.margins.left + background.margins.left + background.margins.right;
        }
        /**
        * Calculates the height of one row based on the text height and icon size.
        * @return Row height
        **/
        function calcRowHeight() {
            var textElement = Qt.createQmlObject(
                'import Qt 4.7;'
                + 'Text {\n'
                + '     text: "Some Text"\n'
                + '     font.bold: true\n'
                + '     visible: false\n'
                + '}',
                listView, "calcRowHeight");
            var height = textElement.height;
            textElement.destroy();
            // icon size or two text elements and margins and hoverItem margins
            return Math.max(32, height*2 + informativeTabBox.textMargin * 3 + hoverItem.margins.top + hoverItem.margins.bottom);
        }
        signal currentIndexChanged(int index)
        id: listView
        objectName: "listView"
        // the maximum text width + icon item width (32 + 4 margin) + margins for hover item + margins for background
        property int maxRowWidth: calculateMaxRowWidth()
        property int rowHeight: calcRowHeight()
        // used for image provider URL to trick Qt into reloading icons when the model changes
        property int imageId: 0
        anchors {
            fill: parent
            topMargin: background.margins.top
            leftMargin: background.margins.left
            rightMargin: background.margins.right
            bottomMargin: background.margins.bottom
        }
        clip: true
        delegate: listDelegate
        highlight: PlasmaCore.FrameSvgItem {
            id: highlightItem
            imagePath: "widgets/viewitem"
            prefix: "hover"
            width: listView.width
        }
        highlightMoveDuration: 250
    }
}
