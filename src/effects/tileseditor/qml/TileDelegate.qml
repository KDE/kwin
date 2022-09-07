/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.4
import QtGraphicalEffects 1.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.KWin.Effect.WindowView 1.0
import org.kde.kitemmodels 1.0 as KitemModels

Item {
    id: delegate
    required property KWinComponents.Tile tile

    x: Math.round(tile.absoluteGeometry.x)
    y: Math.round(tile.absoluteGeometry.y)
    z: (tile.layoutDirection === KWinComponents.Tile.Floating ? 1 : 0) + (focus ? 10 : 0)
    //onZChanged: print(delegate + " "+z)
    width: Math.round(tile.absoluteGeometry.width)
    height: Math.round(tile.absoluteGeometry.height)
    ResizeHandle {
        anchors {
            horizontalCenter: parent.left
            top: parent.top
            bottom: parent.bottom
        }
        tile: delegate.tile
        edge: Qt.LeftEdge
    }
    ResizeHandle {
        anchors {
            verticalCenter: parent.top
            left: parent.left
            right: parent.right
        }
        tile: delegate.tile
        edge: Qt.TopEdge
    }
    ResizeHandle {
        anchors {
            horizontalCenter: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        tile: delegate.tile
        edge: Qt.RightEdge
        visible: tile.layoutDirection === KWinComponents.Tile.Floating
    }
    ResizeHandle {
        anchors {
            verticalCenter: parent.bottom
            left: parent.left
            right: parent.right
        }
        tile: delegate.tile
        edge: Qt.BottomEdge
        visible: tile.layoutDirection === KWinComponents.Tile.Floating
    }



    Item {
        anchors.fill: parent

        Rectangle {
            anchors {
                fill: parent
                margins: PlasmaCore.Units.smallSpacing
            }
            visible: tile.tiles.length === 0
            radius: 3
            opacity: tile.layoutDirection === KWinComponents.Tile.Floating ? 0.6 : 0.3
            color: tile.layoutDirection === KWinComponents.Tile.Floating ? PlasmaCore.Theme.backgroundColor : "transparent"
            border.color: PlasmaCore.Theme.textColor
            Rectangle {
                anchors {
                    fill: parent
                    margins: 1
                }
                radius: 3
                color: "transparent"
                border.color: PlasmaCore.Theme.backgroundColor
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                enabled: tile.layoutDirection === KWinComponents.Tile.Floating
                onClicked: delegate.focus = true
                property point lastPos
                onPressed: {
                    lastPos = mapToItem(null, mouse.x, mouse.y);
                }
                onPositionChanged: {
                    if (!pressed) {
                        return;
                    }
                    let pos = mapToItem(null, mouse.x, mouse.y);
                    tile.moveByPixels(Qt.point(pos.x - lastPos.x, pos.y - lastPos.y));
                    lastPos = pos;
                }
            }
        }
        ColumnLayout {
            anchors.centerIn: parent
            visible: tile.tiles.length === 0
            PlasmaComponents.Button {
                Layout.fillWidth: true
                icon.name: "view-split-left-right"
                text: i18n("Split Horizontally")
                onClicked: {tile.split(KWinComponents.Tile.Horizontal)
                    print(KWinComponents.Tile.Horizontal)
                }
            }
            PlasmaComponents.Button {
                Layout.fillWidth: true
                icon.name: "view-split-top-bottom"
                text: i18n("Split Vertically")
                onClicked: tile.split(KWinComponents.Tile.Vertical)
            }
            PlasmaComponents.Button {
                Layout.fillWidth: true
                visible: tile.layoutDirection !== KWinComponents.Tile.Floating
                icon.name: "window-duplicate"
                text: i18n("Add Floating Tile")
                onClicked: tile.split(KWinComponents.Tile.Floating)
            }
            PlasmaComponents.Button {
                Layout.fillWidth: true
                visible: tile.layoutDirection !== KWinComponents.Tile.Floating
                icon.name: "window-maximize"
                text: i18n("Maximize Area")
                checkable: true
                checked: tile.isMaximizeArea
                onCheckedChanged: tile.isMaximizeArea = checked;
            }
            PlasmaComponents.Button {
                id: deleteButton
                visible: tile.canBeRemoved
                Layout.fillWidth: true
                icon.name: "edit-delete"
                text: i18n("Delete")
                onClicked: {
                    tile.remove();
                }
            }
        }
    }
    PlasmaComponents.Button {
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: PlasmaCore.Units.smallSpacing
        }
        visible: tile.layoutDirection === KWinComponents.Tile.Floating && tile.isLayout
        icon.name: "window-duplicate"
        text: i18n("Add Floating Tile")
        onClicked: tile.split(KWinComponents.Tile.Floating)
    }
}
