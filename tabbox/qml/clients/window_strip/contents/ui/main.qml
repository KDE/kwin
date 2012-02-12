/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2011 Marco Martin <mart@kde.org>

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
import org.kde.plasma.components 0.1 as PlasmaComponents
import org.kde.plasma.mobilecomponents 0.1 as MobileComponents
import org.kde.qtextracomponents 0.1
import org.kde.kwin 0.1 as KWin

Item {
    id: thumbnailTabBox
    property int screenWidth
    property bool canStretchX: false
    property bool canStretchY: false
    width: screenWidth
    height: 150
    clip: true


    function setModel(model) {
        thumbnailListView.model = model;
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
    PlasmaCore.Svg {
        id: iconsSvg
        imagePath: "widgets/configuration-icons"
    }

    ListView {
        signal currentIndexChanged(int index)
        id: thumbnailListView
        objectName: "listView"
        orientation: ListView.Horizontal
        height: parent.height
        spacing: 10
        anchors.fill: parent
        clip: true
        delegate: Item {
            id: delegateItem
            width: thumbnailListView.height * 1.6 + 48
            height: thumbnailListView.height
            KWin.ThumbnailItem {
                id: thumbnailItem
                wId: windowId
                width: parent.width - closeButtonContainer.width - 20
                height: thumbnailListView.height - windowTitle.height - 16
                clip: false
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    bottom: windowTitle.top
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        thumbnailListView.currentIndex = index;
                        thumbnailListView.currentIndexChanged(index);
                    }
                }
            }
            PlasmaComponents.Label {
                id: windowTitle
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter

                height: paintedHeight

                text: caption
                elide: Text.ElideRight
                color: theme.textColor
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
            }
            PlasmaCore.FrameSvgItem {
                id: closeButtonContainer
                imagePath: "widgets/button"
                prefix: "shadow"
                width: closeButton.width + margins.left + margins.right
                height: closeButton.height + margins.top + margins.bottom
                visible: closeable
                anchors {
                    top: parent.top
                    right: parent.right
                    topMargin: 32
                }

                PlasmaCore.FrameSvgItem {
                    id: closeButton
                    imagePath: "widgets/button"
                    prefix: "normal"
                    //a bit more left margin
                    width: closeButtonSvg.width + margins.left + margins.right + 16
                    height: closeButtonSvg.height + margins.top + margins.bottom
                    x: parent.margins.left
                    y: parent.margins.top

                    MobileComponents.ActionButton {
                        id: closeButtonSvg
                        svg: iconsSvg
                        iconSize: 22
                        backgroundVisible: false
                        elementId: "close"

                        anchors {
                            verticalCenter: parent.verticalCenter
                            right: parent.right
                            rightMargin: parent.margins.right
                        }

                        onClicked: thumbnailListView.model.close(index)
                    }
                }
            }
        }
    }
}
