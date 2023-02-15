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
import org.kde.kitemmodels 1.0 as KitemModels

FocusScope {
    id: root
    focus: true

    Keys.onPressed: {
        switch(event.key) {
        case Qt.Key_Escape:
            root.active = false;
            effect.deactivate(effect.animationDuration);
            break;
        case Qt.Key_L:
            if (event.modifiers == Qt.AltModifier) {
                loadLayoutDialog.open();
            }
            break;
        default:
            break;
        }
    }

    required property QtObject effect
    required property QtObject targetScreen

    property bool active: false

    Component.onCompleted: {
        root.active = true;
    }

    function stop() {
        active = false;
    }

    MouseArea {
        anchors.fill: parent
        onClicked: effect.deactivate(effect.animationDuration);
    }

    Item {
        id: blurredWindows
        anchors.fill: parent

        Repeater {
            model: KWinComponents.ClientFilterModel {
                activity: KWinComponents.Workspace.currentActivity
                desktop: KWinComponents.Workspace.currentVirtualDesktop
                screenName: targetScreen.name
                clientModel: KWinComponents.ClientModel {}
            }

            KWinComponents.WindowThumbnailItem {
                wId: model.client.internalId
                x: model.client.x - targetScreen.geometry.x
                y: model.client.y - targetScreen.geometry.y
                width: model.client.width
                height: model.client.height
                z: model.client.stackingOrder
                visible: !model.client.minimized
            }
        }
        property real blurRadius: root.active ? 64 : 0

        layer.enabled: true
        layer.effect: FastBlur {
            radius: blurredWindows.blurRadius
            Behavior on radius {
                NumberAnimation {
                    duration: effect.animationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: PlasmaCore.Theme.backgroundColor
        opacity: root.active ? 0.4 : 0
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    TileDelegate {
        tile: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile
        visible: tilesRepeater.count === 0 || tile.layoutDirection === KWinComponents.Tile.Floating
    }

    Item {
        anchors.fill: parent
        opacity: root.active ? 1 : 0
        z: 1
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        Repeater {
            id: tilesRepeater
            model: KitemModels.KDescendantsProxyModel {
                model: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).model
            }
            delegate: TileDelegate {}
        }
    }

    PlasmaComponents.Control {
        z: 2
        anchors.right: parent.right
        y: root.active ? 0 : -height
        leftPadding: background.margins.left
        topPadding: PlasmaCore.Units.smallSpacing
        rightPadding: PlasmaCore.Units.smallSpacing
        bottomPadding: background.margins.bottom
        opacity: root.active
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on y {
            YAnimator {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
        contentItem: RowLayout {
            PlasmaComponents.Label {
                text: i18nd("kwin_effects","Padding:")
            }
            PlasmaComponents.SpinBox {
                from: 0
                to: PlasmaCore.Units.gridUnit * 2
                value: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile.padding
                onValueModified: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile.padding = value
            }
            PlasmaComponents.Button {
                icon.name: "document-open"
                text: i18nd("kwin_effects","Load Layout...")
                onClicked: loadLayoutDialog.open()
                // This mouse area is for fitts law
                MouseArea {
                    anchors {
                        fill: parent
                        margins: -PlasmaCore.Units.smallSpacing
                    }
                    z: -1
                    onClicked: parent.clicked()
                }
            }
        }
        background: PlasmaCore.FrameSvgItem {
            imagePath: "widgets/background"
            enabledBorders: PlasmaCore.FrameSvg.LeftBorder | PlasmaCore.FrameSvg.BottomBorder
        }
    }
    PlasmaComponents.Popup {
        id: loadLayoutDialog
        x: Math.round(parent.width / 2 - width / 2)
        y: Math.round(parent.height / 2 - height / 2)
        width: Math.min(root.width - PlasmaCore.Units.gridUnit * 4, implicitWidth)
        height: Math.round(implicitHeight * (width / implicitWidth))

        modal: true
        dim: true
        onOpened: forceActiveFocus()
        onClosed: root.forceActiveFocus()
        PlasmaCore.Svg {
            id: layoutsSvg
            imagePath: Qt.resolvedUrl("layouts.svg")
        }
        component LayoutButton: PlasmaComponents.AbstractButton {
            id: button
            Layout.fillWidth: true
            Layout.fillHeight: true
            property alias image: svgItem.elementId
            contentItem: PlasmaCore.SvgItem {
                id: svgItem
                svg: layoutsSvg
                implicitWidth: naturalSize.width
                implicitHeight: naturalSize.height
            }
            background: PlasmaCore.FrameSvgItem {
                imagePath: "widgets/viewitem"
                prefix: "hover"
                opacity: parent.hovered || parent.focus
            }
            onClicked: loadLayoutDialog.close()
            Keys.onPressed: {
                switch(event.key) {
                case Qt.Key_Escape:
                    loadLayoutDialog.close();
                    break;
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    button.clicked();
                    break;
                default:
                    break;
                }
            }
        }
        contentItem: ColumnLayout {
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                LayoutButton {
                    id: apply2Columns
                    focus: true
                    image: "2columns"
                    KeyNavigation.right: apply3Columns
                    onClicked: {
                        const rootTile = KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile;
                        while (rootTile.tiles.length > 0) {
                            rootTile.tiles[0].remove();
                        }
                        rootTile.split(KWinComponents.Tile.Horizontal)
                        rootTile.tiles[0].relativeGeometry = Qt.rect(0, 0, 0.5, 1);
                    }
                }
                LayoutButton {
                    id: apply3Columns
                    image: "3columns"
                    KeyNavigation.right: apply2ColumnsVerticalSplit
                    KeyNavigation.left: apply2Columns
                    onClicked: {
                        const rootTile = KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile;
                        while (rootTile.tiles.length > 0) {
                            rootTile.tiles[0].remove();
                        }
                        rootTile.split(KWinComponents.Tile.Horizontal)
                        rootTile.tiles[0].split(KWinComponents.Tile.Horizontal)
                        rootTile.tiles[1].relativeGeometry = Qt.rect(0.25, 0, 0.5, 1);
                    }
                }
                LayoutButton {
                    id: apply2ColumnsVerticalSplit
                    image: "2columnsVerticalSplit"
                    KeyNavigation.left: apply3Columns
                    onClicked: {
                        const rootTile = KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile;
                        while (rootTile.tiles.length > 0) {
                            rootTile.tiles[0].remove();
                        }
                        rootTile.split(KWinComponents.Tile.Horizontal)
                        rootTile.tiles[0].relativeGeometry = Qt.rect(0, 0, 0.4, 1);
                        rootTile.tiles[0].split(KWinComponents.Tile.Vertical)
                    }
                }
            }
            PlasmaComponents.Button {
                Layout.alignment: Qt.AlignRight
                text: i18nd("kwin_effects","Close")
                onClicked: loadLayoutDialog.close()
            }
        }
    }
}
