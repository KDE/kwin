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
import QtQuick.Layouts 1.1
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.kquickcontrolsaddons 2.0
import org.kde.kwin 2.0 as KWin

KWin.Switcher {
    id: tabBox
    property real screenFactor: screenGeometry.width/screenGeometry.height
    currentIndex: thumbnailListView.currentIndex

    // just to get the margin sizes
    PlasmaCore.FrameSvgItem {
        id: hoverItem
        imagePath: "widgets/viewitem"
        prefix: "hover"
        visible: false
    }

    PlasmaCore.Dialog {
        id: dialog
        location: PlasmaCore.Types.Floating
        visible: tabBox.visible
        flags: Qt.X11BypassWindowManagerHint
        x: screenGeometry.x
        y: screenGeometry.y

        mainItem: PlasmaExtras.ScrollArea {
            id: dialogMainItem

            property bool canStretchX: false
            property bool canStretchY: false
            width: tabBox.screenGeometry.width * 0.15
            height: tabBox.screenGeometry.height
            clip: true
            focus: true

            ListView {
                id: thumbnailListView
                orientation: ListView.Vertical
                model: tabBox.model
                anchors.fill: parent
                property int delegateWidth: thumbnailListView.width
                spacing: 5
                highlightMoveDuration: 250

                clip: true
                delegate: Item {
                    id: delegateItem
                    width: thumbnailListView.delegateWidth
                    height: Math.round(thumbnailItem.height + label.height + 30)
                    Item {
                        id: thumbnailItem
                        width: parent.width - hoverItem.margins.left - hoverItem.margins.right
                        height: Math.round(thumbnailListView.delegateWidth*(1.0/screenFactor) - hoverItem.margins.top)
                        anchors {
                            top: parent.top
                            left: parent.left
                            leftMargin: hoverItem.margins.left
                            rightMargin: hoverItem.margins.right
                            topMargin: hoverItem.margins.top
                        }
                        KWin.ThumbnailItem {
                            wId: windowId
                            anchors.fill: parent
                        }
                    }
                    RowLayout {
                        id: label
                        spacing: 4
                        property int maximumWidth: thumbnailListView.delegateWidth
                        Layout.maximumWidth: maximumWidth
                        anchors {
                            left: parent.left
                            bottom: parent.bottom
                            leftMargin: hoverItem.margins.left
                            bottomMargin: hoverItem.margins.bottom
                        }
                        QIconItem {
                            id: iconItem
                            icon: model.icon
                            property int iconSize: 32
                            width: iconSize
                            height: iconSize
                            Layout.preferredHeight: iconSize
                            Layout.preferredWidth: iconSize
                        }
                        PlasmaComponents.Label {
                            text: model.caption
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            Layout.maximumWidth: label.maximumWidth - iconItem.iconSize - label.spacing * 2
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            thumbnailListView.currentIndex = index;
                        }
                    }
                }
                highlight: PlasmaCore.FrameSvgItem {
                    id: highlightItem
                    imagePath: "widgets/viewitem"
                    prefix: "hover"
                    width: thumbnailListView.currentItem.width
                    height: thumbnailListView.currentItem.height
                }
                boundsBehavior: Flickable.StopAtBounds
                Connections {
                    target: tabBox
                    onCurrentIndexChanged: {
                        thumbnailListView.currentIndex = tabBox.currentIndex;
                        thumbnailListView.positionViewAtIndex(thumbnailListView.currentIndex, ListView.Contain);
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
