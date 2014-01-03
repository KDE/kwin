/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2013 Marco Martin <mart@kde.org>

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
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.qtextracomponents 2.0
import org.kde.kwin 2.0 as KWin

KWin.Switcher {
    id: tabBox
    property real screenFactor: screenGeometry.width/screenGeometry.height

    PlasmaCore.Dialog {
        id: dialog
        location: PlasmaCore.Types.Floating
        visible: tabBox.visible
        flags: Qt.X11BypassWindowManagerHint
        onVisibleChanged: {
            if (visible) {
                dialog.x = screenGeometry.x;
                dialog.y = screenGeometry.y;
            }
        }

        mainItem: Item {
            id: dialogMainItem
            property int optimalWidth: (thumbnailListView.thumbnailWidth + hoverItem.margins.left + hoverItem.margins.right)

            property bool canStretchX: false
            property bool canStretchY: false
            width: Math.min(Math.max(tabBox.screenGeometry.width * 0.15, optimalWidth), tabBox.screenGeometry.width * 0.3)
            height: tabBox.screenGeometry.height
            clip: true
            focus: true

            // just to get the margin sizes
            PlasmaCore.FrameSvgItem {
                id: hoverItem
                imagePath: "widgets/viewitem"
                prefix: "hover"
                visible: false
            }

            PlasmaExtras.ScrollArea {
                anchors {
                    fill: parent
                }
                ListView {
                    id: thumbnailListView
                    orientation: ListView.Vertical
                    model: tabBox.model
                    property int thumbnailWidth: width
                    height: thumbnailWidth * (1.0/screenFactor) + hoverItem.margins.bottom + hoverItem.margins.top
                    spacing: 5
                    highlightMoveDuration: 250
                    width: Math.min(parent.width - (anchors.leftMargin + anchors.rightMargin) - (hoverItem.margins.left + hoverItem.margins.right), thumbnailWidth * count + 5 * (count - 1))

                    clip: true
                    delegate: Item {
                        property alias data: thumbnailItem.data
                        id: delegateItem
                        width: thumbnailListView.thumbnailWidth
                        height: thumbnailListView.thumbnailWidth*(1.0/screenFactor) + label.height + 30
                        KWin.ThumbnailItem {
                            property variant data: model
                            id: thumbnailItem
                            wId: windowId
                            anchors {
                                centerIn: parent
                            }
                            width: thumbnailListView.thumbnailWidth
                            height: thumbnailListView.thumbnailWidth*(1.0/screenFactor)
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                thumbnailListView.currentIndex = index;
                            }
                        }
                        Row {
                            id: label
                            spacing: 4
                            anchors {
                                left: parent.left
                                bottom: parent.bottom
                                leftMargin: 8
                                bottomMargin: 8
                            }
                            QIconItem {
                                id: iconItem
                                icon: model.icon
                                width: 32
                                height: 32
                            }
                            PlasmaComponents.Label {
                                text: model.caption
                                anchors.verticalCenter: parent.verticalCenter
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
                    boundsBehavior: Flickable.StopAtBounds
                    Connections {
                        target: tabBox
                        onCurrentIndexChanged: {thumbnailListView.currentIndex = tabBox.currentIndex;}
                    }
                }
            }

            /*
            * Key navigation on outer item for two reasons:
            * @li we have to emit the change signal
            * @li on multiple invocation it does not work on the list view. Focus seems to be lost.
            **/
            Keys.onPressed: {
                if (event.key == Qt.Key_Up) {
                    thumbnailListView.decrementCurrentIndex();
                } else if (event.key == Qt.Key_Down) {
                    thumbnailListView.incrementCurrentIndex();
                }
            }
        }
    }
}
