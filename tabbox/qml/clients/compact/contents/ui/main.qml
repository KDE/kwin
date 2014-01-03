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
import org.kde.qtextracomponents 2.0
import org.kde.kwin 2.0 as KWin

KWin.Switcher {
    id: tabBox
    currentIndex: compactListView.currentIndex

    PlasmaCore.Dialog {
        id: dialog
        location: PlasmaCore.Types.Floating
        visible: tabBox.visible
        flags: Qt.X11BypassWindowManagerHint
        x: tabBox.screenGeometry.x + tabBox.screenGeometry.width * 0.5 - dialogMainItem.width * 0.5
        y: tabBox.screenGeometry.y + tabBox.screenGeometry.height * 0.5 - dialogMainItem.height * 0.5

        mainItem: Item {
            id: dialogMainItem
            property int optimalWidth: compactListView.maxRowWidth
            property int optimalHeight: compactListView.rowHeight * compactListView.count
            property bool canStretchX: true
            property bool canStretchY: false
            width: Math.min(Math.max(tabBox.screenGeometry.width * 0.2, optimalWidth), tabBox.screenGeometry.width * 0.8)
            height: Math.min(Math.max(tabBox.screenGeometry.height * 0.2, optimalHeight), tabBox.screenGeometry.height * 0.8)
            focus: true

            property int textMargin: 2

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
                    id: delegateItem
                    width: compactListView.width
                    height: compactListView.rowHeight
                    opacity: minimized ? 0.6 : 1.0
                    QIconItem {
                        id: iconItem
                        icon: model.icon
                        width: 16
                        height: 16
                        anchors {
                            verticalCenter: parent.verticalCenter
                            left: parent.left
                            leftMargin: hoverItem.margins.left
                        }
                    }
                    Text {
                        id: captionItem
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignBottom
                        text: dialogMainItem.itemCaption(caption, minimized)
                        font.bold: index == compactListView.currentIndex
                        color: theme.textColor
                        elide: Text.ElideMiddle
                        anchors {
                            left: iconItem.right
                            right: parent.right
                            top: parent.top
                            bottom: parent.bottom
                            topMargin: hoverItem.margins.top
                            rightMargin: hoverItem.margins.right
                            bottomMargin: hoverItem.margins.bottom
                            leftMargin: 2 * dialogMainItem.textMargin
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            compactListView.currentIndex = index;
                        }
                    }
                }
            }
            ListView {
                function calculateMaxRowWidth() {
                    var width = 0;
                    var textElement = Qt.createQmlObject(
                        'import QtQuick 2.0;'
                        + 'Text {\n'
                        + '     text: "' + dialogMainItem.itemCaption(tabBox.model.longestCaption(), true) + '"\n'
                        + '     font.bold: true\n'
                        + '     visible: false\n'
                        + '}',
                        compactListView, "calculateMaxRowWidth");
                    width = Math.max(textElement.width, width);
                    textElement.destroy();
                    return width + 16 + 2 * dialogMainItem.textMargin + hoverItem.margins.right + hoverItem.margins.left
                }
                /**
                * Calculates the height of one row based on the text height and icon size.
                * @return Row height
                **/
                function calcRowHeight() {
                    var textElement = Qt.createQmlObject(
                        'import QtQuick 2.0;'
                        + 'Text {\n'
                        + '     text: "Some Text"\n'
                        + '     font.bold: true\n'
                        + '     visible: false\n'
                        + '}',
                        compactListView, "calcRowHeight");
                    var height = textElement.height;
                    textElement.destroy();
                    // icon size or two text elements and margins and hoverItem margins
                    return Math.max(16, height + hoverItem.margins.top + hoverItem.margins.bottom);
                }
                id: compactListView
                // the maximum text width + icon item width (32 + 4 margin) + margins for hover item
                property int maxRowWidth: calculateMaxRowWidth()
                property int rowHeight: calcRowHeight()
                anchors {
                    fill: parent
                }
                model: tabBox.model
                clip: true
                delegate: listDelegate
                highlight: PlasmaCore.FrameSvgItem {
                    id: highlightItem
                    imagePath: "widgets/viewitem"
                    prefix: "hover"
                    width: compactListView.width
                }
                highlightMoveDuration: 250
                boundsBehavior: Flickable.StopAtBounds
                Connections {
                    target: tabBox
                    onCurrentIndexChanged: {compactListView.currentIndex = tabBox.currentIndex;}
                }
            }
            /*
            * Key navigation on outer item for two reasons:
            * @li we have to emit the change signal
            * @li on multiple invocation it does not work on the list view. Focus seems to be lost.
            **/
            Keys.onPressed: {
                if (event.key == Qt.Key_Up) {
                    compactListView.decrementCurrentIndex();
                } else if (event.key == Qt.Key_Down) {
                    compactListView.incrementCurrentIndex();
                }
            }
        }
    }
}
