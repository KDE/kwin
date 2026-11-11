/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

Item {
    id: delegate
    required property KWinComponents.Tile tile

    x: Math.round(tile.absoluteGeometryInScreen.x)
    y: Math.round(tile.absoluteGeometryInScreen.y)
    z: focus ? 1000 : 0
    width: Math.round(tile.absoluteGeometryInScreen.width)
    height: Math.round(tile.absoluteGeometryInScreen.height)

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

    ResizeCorner {
        anchors {
            top: parent.top
            left: parent.left
        }
        tile: delegate.tile
        corner: Qt.TopLeftCorner
    }
    ResizeCorner {
        anchors {
            top: parent.top
            right: parent.right
        }
        tile: delegate.tile
        corner: Qt.TopRightCorner
    }
    ResizeCorner {
        anchors {
            left: parent.left
            bottom: parent.bottom
        }
        tile: delegate.tile
        corner: Qt.BottomLeftCorner
    }
    ResizeCorner {
        anchors {
            right: parent.right
            bottom: parent.bottom
        }
        tile: delegate.tile
        corner: Qt.BottomRightCorner
    }

    Item {
        anchors.fill: parent

        Rectangle {
            anchors {
                fill: parent
                margins: Kirigami.Units.smallSpacing
            }
            visible: tile.tiles.length === 0
            radius: Kirigami.Units.cornerRadius
            opacity: 0.3
            color: "transparent"
            border.color: Kirigami.Theme.textColor
            Rectangle {
                anchors {
                    fill: parent
                    margins: 1
                }
                radius: Kirigami.Units.cornerRadius
                color: "transparent"
                border.color: Kirigami.Theme.backgroundColor
            }
        }
        GridLayout {
            anchors.centerIn: parent
            visible: tile.tiles.length === 0
            readonly property bool compact: delegate.width < Kirigami.Units.gridUnit * 10 || delegate.height < splitButton.implicitHeight * visibleChildren.length + rowSpacing * visibleChildren.length + Kirigami.Units.gridUnit
            rows: compact ? 1 : -1
            columns: compact ? -1 : 1
            PlasmaComponents.Button {
                id: splitButton
                Layout.fillWidth: true
                icon.name: "view-split-left-right"
                text: i18nd("kwin","Split Left/Right")
                display: parent.compact ? PlasmaComponents.Button.IconOnly : PlasmaComponents.Button.TextBesideIcon
                onClicked: tile.split(KWinComponents.Tile.Horizontal)
            }
            PlasmaComponents.Button {
                Layout.fillWidth: true
                icon.name: "view-split-top-bottom"
                text: i18nd("kwin","Split Top/Bottom")
                display: parent.compact ? PlasmaComponents.Button.IconOnly : PlasmaComponents.Button.TextBesideIcon
                onClicked: tile.split(KWinComponents.Tile.Vertical)
            }
            PlasmaComponents.Button {
                id: deleteButton
                visible: tile.canBeRemoved
                Layout.fillWidth: true
                icon.name: "edit-delete"
                text: i18nd("kwin","Delete")
                display: parent.compact ? PlasmaComponents.Button.IconOnly : PlasmaComponents.Button.TextBesideIcon
                onClicked: {
                    tile.remove();
                }
            }
        }
    }
}
