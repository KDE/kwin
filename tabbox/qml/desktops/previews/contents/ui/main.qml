/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
    id: desktopTabBox
    property int screenWidth : 1
    property int screenHeight : 1
    property real screenFactor: screenWidth/screenHeight
    property bool allDesktops: true
    property int optimalWidth: (listView.thumbnailWidth + hoverItem.margins.left + hoverItem.margins.right) * listView.count + background.leftMargin + background.bottomMargin
    property int optimalHeight: listView.thumbnailWidth*(1.0/screenFactor) + hoverItem.margins.top + hoverItem.margins.bottom + background.topMargin + background.bottomMargin + 40
    property bool canStretchX: true
    property bool canStretchY: false
    property string maskImagePath: background.maskImagePath
    property double maskWidth: background.centerWidth
    property double maskHeight: background.centerHeight
    property int maskTopMargin: background.centerTopMargin
    property int maskLeftMargin: background.centerLeftMargin
    width: Math.min(Math.max(screenWidth * 0.2, optimalWidth), screenWidth * 0.8)
    height: Math.min(optimalHeight, screenHeight * 0.8)

    property int textMargin: 2

    function setModel(model) {
        listView.model = model;
    }

    function modelChanged() {
        listView.currentIndex = -1;
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

    ShadowedSvgItem {
        id: background
        anchors.fill: parent
    }

    ListView {
        signal currentIndexChanged(int index)
        id: listView
        property int thumbnailWidth: 600
        height: thumbnailWidth * (1.0/screenFactor) + hoverItem.margins.bottom + hoverItem.margins.top
        width: Math.min(parent.width - (anchors.leftMargin + anchors.rightMargin) - (hoverItem.margins.left + hoverItem.margins.right), thumbnailWidth * count + 5 * (count - 1))
        spacing: 5
        orientation: ListView.Horizontal
        objectName: "listView"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: background.topMargin
            leftMargin: background.leftMargin
            rightMargin: background.rightMargin
        }
        clip: true
        highlight: PlasmaCore.FrameSvgItem {
            id: highlightItem
            imagePath: "widgets/viewitem"
            prefix: "hover"
            width: listView.thumbnailWidth
            height: listView.thumbnailWidth*(1.0/screenFactor)
        }
        delegate: Item {
            property alias data: thumbnailItem.data
            width: listView.thumbnailWidth
            height: listView.thumbnailWidth*(1.0/screenFactor)
            KWin.DesktopThumbnailItem {
                id: thumbnailItem
                property variant data: model
                clip: true
                desktop: model.desktop
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
                    listView.currentIndex = index;
                    listView.currentIndexChanged(listView.currentIndex);
                }
            }
        }
        highlightMoveDuration: 250
        boundsBehavior: Flickable.StopAtBounds
    }
    Item {
        height: 40
        id: captionFrame
        anchors {
            top: listView.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            topMargin: hoverItem.margins.bottom
            leftMargin: background.leftMargin
            rightMargin: background.rightMargin
            bottomMargin: background.bottomMargin
        }
        Text {
            function constrainWidth() {
                if (textItem.width > textItem.maxWidth && textItem.width > 0 && textItem.maxWidth > 0) {
                    textItem.width = textItem.maxWidth;
                } else {
                    textItem.width = undefined;
                }
            }
            function calculateMaxWidth() {
                textItem.maxWidth = desktopTabBox.width - captionFrame.anchors.leftMargin - captionFrame.anchors.rightMargin - - captionFrame.anchors.rightMargin;
            }
            id: textItem
            property int maxWidth: 0
            text: listView.currentItem ? listView.currentItem.data.caption : ""
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: theme.textColor
            elide: Text.ElideMiddle
            font {
                bold: true
            }
            anchors {
                verticalCenter: parent.verticalCenter
                horizontalCenter: parent.horizontalCenter
            }
            onTextChanged: textItem.constrainWidth()
            Component.onCompleted: textItem.calculateMaxWidth()
            Connections {
                target: desktopTabBox
                onWidthChanged: {
                    textItem.calculateMaxWidth();
                    textItem.constrainWidth();
                }
            }
        }
    }
}
