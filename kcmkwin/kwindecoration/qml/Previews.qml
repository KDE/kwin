/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.1
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import org.kde.kwin.private.kdecoration 1.0 as KDecoration

ScrollView {
    objectName: "themeList"
    frameVisible: true
    GridView {
        id: gridView
        objectName: "listView"
        model: decorationsModel
        cellWidth: 20 * units.gridUnit
        cellHeight: cellWidth / 1.6
        onContentHeightChanged: {
            if (gridView.currentIndex == -1) {
                gridView.currentIndex = savedIndex;
            }
            gridView.positionViewAtIndex(gridView.currentIndex, GridView.Visible);
        }

        Rectangle {
            z: -1
            anchors.fill: parent
            color: baseColor
        }
        highlight: Rectangle {
            color: highlightColor
            opacity: 0.6
        }
        highlightMoveDuration: units.longDuration
        boundsBehavior: Flickable.StopAtBounds
        property int borderSizesIndex: 3 // 3 == Normal
        delegate: Item {
            width: gridView.cellWidth
            height: gridView.cellHeight
            KDecoration.Bridge {
                id: bridgeItem
                plugin: model["plugin"]
                theme: model["theme"]
            }
            KDecoration.Settings {
                id: settingsItem
                bridge: bridgeItem.bridge
                borderSizesIndex: gridView.borderSizesIndex
            }
            MouseArea {
                hoverEnabled: false
                anchors.fill: parent
                onClicked: {
                    gridView.currentIndex = index;
                }
            }
            ColumnLayout {
                id: decorationPreviews
                property string themeName: model["display"]
                anchors.fill: parent
                Item {
                    KDecoration.Decoration {
                        id: inactivePreview
                        bridge: bridgeItem.bridge
                        settings: settingsItem
                        anchors.fill: parent
                        Component.onCompleted: {
                            client.caption = decorationPreviews.themeName
                            client.active = false;
                            anchors.leftMargin = Qt.binding(function() { return 40 - (inactivePreview.shadow ? inactivePreview.shadow.paddingLeft : 0);});
                            anchors.rightMargin = Qt.binding(function() { return 10 - (inactivePreview.shadow ? inactivePreview.shadow.paddingRight : 0);});
                            anchors.topMargin = Qt.binding(function() { return 10 - (inactivePreview.shadow ? inactivePreview.shadow.paddingTop : 0);});
                            anchors.bottomMargin = Qt.binding(function() { return 40 - (inactivePreview.shadow ? inactivePreview.shadow.paddingBottom : 0);});
                        }
                    }
                    KDecoration.Decoration {
                        id: activePreview
                        bridge: bridgeItem.bridge
                        settings: settingsItem
                        anchors.fill: parent
                        Component.onCompleted: {
                            client.caption = decorationPreviews.themeName
                            client.active = true;
                            anchors.leftMargin = Qt.binding(function() { return 10 - (activePreview.shadow ? activePreview.shadow.paddingLeft : 0);});
                            anchors.rightMargin = Qt.binding(function() { return 40 - (activePreview.shadow ? activePreview.shadow.paddingRight : 0);});
                            anchors.topMargin = Qt.binding(function() { return 40 - (activePreview.shadow ? activePreview.shadow.paddingTop : 0);});
                            anchors.bottomMargin = Qt.binding(function() { return 10 - (activePreview.shadow ? activePreview.shadow.paddingBottom : 0);});
                        }
                    }
                    MouseArea {
                        hoverEnabled: false
                        anchors.fill: parent
                        onClicked: {
                            gridView.currentIndex = index;
                        }
                    }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    /* Create an invisible rectangle that's the same size as the inner content rect
                       of the foreground window preview so that we can center the button within it.
                       We have to center rather than using anchors because the button width varies
                       with different localizations */
                    Item {
                        anchors {
                            left: parent.left
                            leftMargin: 10
                            right: parent.right
                            rightMargin: 40
                            top: parent.top
                            topMargin: 68
                            bottom: parent.bottom
                            bottomMargin: 10
                        }
                        Button {
                            id: configureButton
                            anchors {
                                horizontalCenter: parent.horizontalCenter
                                verticalCenter: parent.verticalCenter
                            }
                            enabled: model["configureable"]
                            iconName: "configure"
                            text: i18n("Configure %1...", decorationPreviews.themeName)
                            onClicked: {
                                gridView.currentIndex = index
                                bridgeItem.bridge.configure()
                            }
                        }
                    }
                }
            }
        }
    }
    Layout.preferredHeight: 20 * units.gridUnit
}

