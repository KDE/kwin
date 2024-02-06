/*
 KWin - the KDE window manager
 This file is part of the KDE project.

 SPDX-FileCopyrightText: 2020 Chris Holland <zrenfire@gmail.com>
 SPDX-FileCopyrightText: 2023 Nate Graham <nate@kde.org>

 SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts 1.1
import org.kde.plasma.core as PlasmaCore
import org.kde.ksvg 1.0 as KSvg
import org.kde.plasma.components 3.0 as PlasmaComponents3
import org.kde.kwin 3.0 as KWin
import org.kde.kirigami 2.20 as Kirigami

KWin.TabBoxSwitcher {
    id: tabBox

    Instantiator {
        active: tabBox.visible
        delegate: PlasmaCore.Dialog {
            location: PlasmaCore.Types.Floating
            visible: true
            flags: Qt.X11BypassWindowManagerHint
            x: tabBox.screenGeometry.x + tabBox.screenGeometry.width * 0.5 - dialogMainItem.width * 0.5
            y: tabBox.screenGeometry.y + tabBox.screenGeometry.height * 0.5 - dialogMainItem.height * 0.5

            mainItem: FocusScope {
                id: dialogMainItem

                focus: true

                property int maxWidth: tabBox.screenGeometry.width * 0.9
                property int maxHeight: tabBox.screenGeometry.height * 0.7
                property real screenFactor: tabBox.screenGeometry.width / tabBox.screenGeometry.height
                property int maxGridColumnsByWidth: Math.floor(maxWidth / thumbnailGridView.cellWidth)

                property int gridColumns: {         // Simple greedy algorithm
                    // respect screenGeometry
                    const c = Math.min(thumbnailGridView.count, maxGridColumnsByWidth);
                    const residue = thumbnailGridView.count % c;
                    if (residue == 0) {
                        return c;
                    }
                    // start greedy recursion
                    return columnCountRecursion(c, c, c - residue);
                }

                property int gridRows: Math.ceil(thumbnailGridView.count / gridColumns)
                property int optimalWidth: thumbnailGridView.cellWidth * gridColumns
                property int optimalHeight: thumbnailGridView.cellHeight * gridRows
                width: Math.min(Math.max(thumbnailGridView.cellWidth, optimalWidth), maxWidth)
                height: Math.min(Math.max(thumbnailGridView.cellHeight, optimalHeight), maxHeight)

                clip: true

                // Step for greedy algorithm
                function columnCountRecursion(prevC, prevBestC, prevDiff) {
                    const c = prevC - 1;

                    // don't increase vertical extent more than horizontal
                    // and don't exceed maxHeight
                    if (prevC * prevC <= thumbnailGridView.count + prevDiff ||
                            maxHeight < Math.ceil(thumbnailGridView.count / c) * thumbnailGridView.cellHeight) {
                        return prevBestC;
                    }
                    const residue = thumbnailGridView.count % c;
                    // halts algorithm at some point
                    if (residue == 0) {
                        return c;
                    }
                    // empty slots
                    const diff = c - residue;

                    // compare it to previous count of empty slots
                    if (diff < prevDiff) {
                        return columnCountRecursion(c, c, diff);
                    } else if (diff == prevDiff) {
                        // when it's the same try again, we'll stop early enough thanks to the landscape mode condition
                        return columnCountRecursion(c, prevBestC, diff);
                    }
                    // when we've found a local minimum choose this one (greedy)
                    return columnCountRecursion(c, prevBestC, diff);
                }

                // Just to get the margin sizes
                KSvg.FrameSvgItem {
                    id: hoverItem
                    imagePath: "widgets/viewitem"
                    prefix: "hover"
                    visible: false
                }

                GridView {
                    id: thumbnailGridView
                    anchors.fill: parent
                    focus: true
                    model: tabBox.model
                    currentIndex: tabBox.currentIndex

                    readonly property int iconSize: Kirigami.Units.iconSizes.huge
                    readonly property int captionRowHeight: Kirigami.Units.gridUnit * 2
                    readonly property int columnSpacing: Kirigami.Units.gridUnit
                    readonly property int thumbnailWidth: Kirigami.Units.gridUnit * 16
                    readonly property int thumbnailHeight: thumbnailWidth * (1.0/dialogMainItem.screenFactor)
                    cellWidth: hoverItem.margins.left + thumbnailWidth + hoverItem.margins.right
                    cellHeight: hoverItem.margins.top + captionRowHeight + thumbnailHeight + hoverItem.margins.bottom

                    keyNavigationWraps: true
                    highlightMoveDuration: 0

                    delegate: MouseArea {
                        id: thumbnailGridItem
                        width: thumbnailGridView.cellWidth
                        height: thumbnailGridView.cellHeight
                        focus: GridView.isCurrentItem
                        hoverEnabled: true

                        Accessible.name: model.caption
                        Accessible.role: Accessible.ListItem

                        onClicked: {
                            tabBox.currentIndex = index;
                        }

                        ColumnLayout {
                            id: columnLayout
                            z: 0
                            spacing: thumbnailGridView.columnSpacing
                            anchors.fill: parent
                            anchors.leftMargin: hoverItem.margins.left * 2
                            anchors.topMargin: hoverItem.margins.top * 2
                            anchors.rightMargin: hoverItem.margins.right * 2
                            anchors.bottomMargin: hoverItem.margins.bottom * 2


                            // KWin.WindowThumbnail needs a container
                            // otherwise it will be drawn the same size as the parent ColumnLayout
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                KWin.WindowThumbnail {
                                    anchors.fill: parent
                                    wId: windowId
                                }

                                Kirigami.Icon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.verticalCenter: parent.bottom
                                    anchors.verticalCenterOffset: Math.round(-thumbnailGridView.iconSize / 4)
                                    width: thumbnailGridView.iconSize
                                    height: thumbnailGridView.iconSize

                                    source: model.icon
                                }

                                PlasmaComponents3.ToolButton {
                                    id: closeButton
                                    anchors {
                                        right: parent.right
                                        top: parent.top
                                        // Deliberately touch the inner edges of the frame
                                        rightMargin: -columnLayout.anchors.rightMargin
                                        topMargin: -columnLayout.anchors.topMargin
                                    }
                                    visible: model.closeable && typeof tabBox.model.close !== 'undefined' &&
                                            (thumbnailGridItem.containsMouse
                                            || closeButton.hovered
                                            || thumbnailGridItem.focus
                                            || Kirigami.Settings.tabletMode
                                            || Kirigami.Settings.hasTransientTouchInput
                                            )
                                    icon.name: 'window-close-symbolic'
                                    onClicked: {
                                        tabBox.model.close(index);
                                    }
                                }
                            }

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                text: model.caption
                                font.weight: thumbnailGridItem.focus ? Font.Bold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                            }
                        }
                    } // GridView.delegate

                    highlight: KSvg.FrameSvgItem {
                        imagePath: "widgets/viewitem"
                        prefix: "hover"
                    }

                    onCurrentIndexChanged: tabBox.currentIndex = thumbnailGridView.currentIndex;
                } // GridView

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.largeSpacing * 2
                    icon.source: "edit-none"
                    text: i18ndc("kwin", "@info:placeholder no entries in the task switcher", "No open windows")
                    visible: thumbnailGridView.count === 0
                }

                Keys.onPressed: {
                    if (event.key == Qt.Key_Left) {
                        thumbnailGridView.moveCurrentIndexLeft();
                    } else if (event.key == Qt.Key_Right) {
                        thumbnailGridView.moveCurrentIndexRight();
                    } else if (event.key == Qt.Key_Up) {
                        thumbnailGridView.moveCurrentIndexUp();
                    } else if (event.key == Qt.Key_Down) {
                        thumbnailGridView.moveCurrentIndexDown();
                    } else {
                        return;
                    }

                    thumbnailGridView.currentIndexChanged(thumbnailGridView.currentIndex);
                }
            } // Dialog.mainItem
        } // Dialog
    } // Instantiator
}
