/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.kirigami 2.20 as Kirigami
import org.kde.ksvg 1.0 as KSvg
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kitemmodels as KitemModels

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

    readonly property QtObject rootTile: KWinComponents.Workspace.rootTile(root.targetScreen, KWinComponents.Workspace.currentDesktop)

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
            model: KWinComponents.WindowFilterModel {
                activity: KWinComponents.Workspace.currentActivity
                desktop: KWinComponents.Workspace.currentDesktop
                screenName: targetScreen.name
                windowModel: KWinComponents.WindowModel {}
            }

            KWinComponents.WindowThumbnail {
                wId: model.window.internalId
                x: model.window.x - targetScreen.geometry.x
                y: model.window.y - targetScreen.geometry.y
                width: model.window.width
                height: model.window.height
                z: model.window.stackingOrder
                visible: !model.window.minimized
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
        color: Kirigami.Theme.backgroundColor
        opacity: root.active ? 0.4 : 0
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    TileDelegate {
        tile: rootTile
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
                model: rootTile.model
            }
            delegate: TileDelegate {}
        }
    }

    PlasmaComponents.Control {
        z: 2
        anchors.right: parent.right
        y: root.active ? 0 : -height
        leftPadding: background.margins.left
        topPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
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
                text: i18nd("kwin","Padding:")
            }
            PlasmaComponents.SpinBox {
                from: 0
                to: Kirigami.Units.gridUnit * 2
                value: rootTile.padding
                onValueModified: rootTile.padding = value
            }
            PlasmaComponents.Button {
                icon.name: "document-open"
                text: i18nd("kwin","Load Layout…")
                onClicked: loadLayoutDialog.open()
                // This mouse area is for fitts law
                MouseArea {
                    anchors {
                        fill: parent
                        margins: -Kirigami.Units.smallSpacing
                    }
                    z: -1
                    onClicked: parent.clicked()
                }
            }
        }
        background: KSvg.FrameSvgItem {
            imagePath: "widgets/background"
            enabledBorders: KSvg.FrameSvg.LeftBorder | KSvg.FrameSvg.BottomBorder
        }
    }
    PlasmaComponents.Popup {
        id: loadLayoutDialog
        x: Math.round(parent.width / 2 - width / 2)
        y: Math.round(parent.height / 2 - height / 2)
        width: Math.min(root.width - Kirigami.Units.gridUnit * 4, implicitWidth)
        height: Math.round(implicitHeight * (width / implicitWidth))

        modal: true
        dim: true
        onOpened: forceActiveFocus()
        onClosed: root.forceActiveFocus()
        component LayoutButton: PlasmaComponents.AbstractButton {
            id: button
            Layout.fillWidth: true
            Layout.fillHeight: true
            property alias image: svgItem.elementId
            contentItem: KSvg.SvgItem {
                id: svgItem
                imagePath: Qt.resolvedUrl("layouts.svg")
                implicitWidth: naturalSize.width
                implicitHeight: naturalSize.height
            }
            background: KSvg.FrameSvgItem {
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
                text: i18nd("kwin","Close")
                onClicked: loadLayoutDialog.close()
            }
        }
    }
}
