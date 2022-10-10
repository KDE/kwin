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

FocusScope {
    id: root
    focus: true

    Keys.onEscapePressed: {
        root.active = false;
        effect.deactivate(effect.animationDuration);
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
                visible: !model.client.minimized //&& !model.client.tile
            }
        }
        property real blurRadius: root.active ? 64 : 0

        layer.enabled: true//effect.blurBackground
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

    Item {
        id: tiledWindows
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
                visible: !model.client.minimized && model.client.tile
            }
        }
        property real blurRadius: root.active ? 24 : 0
        opacity: root.active ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        layer.enabled: true
        layer.effect: FastBlur {
            radius: tiledWindows.blurRadius
            Behavior on radius {
                NumberAnimation {
                    duration: effect.animationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    TileDelegate {
        tile: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name, KWinComponents.Workspace.currentVirtualDesktop, KWinComponents.Workspace.currentActivity).rootTile
        visible: tilesRepeater.count === 0
    }
    Item {
        anchors.fill: parent
        opacity: root.active ? 1 : 0
        z: 999
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        Repeater {
            id: tilesRepeater
            model: KitemModels.KDescendantsProxyModel {
                model: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name, KWinComponents.Workspace.currentVirtualDesktop, KWinComponents.Workspace.currentActivity)
            }
            delegate: TileDelegate {}
        }

        PlasmaComponents.Control {
            anchors {
                right: parent.right
                top: parent.top
            }
            leftPadding: background.margins.left
            topPadding: PlasmaCore.Units.smallSpacing
            rightPadding: PlasmaCore.Units.smallSpacing
            bottomPadding: background.margins.bottom
            contentItem: RowLayout {
                PlasmaComponents.Label {
                    text: i18n("Padding:")
                }
                PlasmaComponents.SpinBox {
                    from: 0
                    to: PlasmaCore.Units.gridUnit * 2
                    value: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile.padding
                    onValueModified: KWinComponents.Workspace.tilingForScreen(root.targetScreen.name).rootTile.padding = value
                }
            }
            background: PlasmaCore.FrameSvgItem {
                imagePath: "widgets/background"
                enabledBorders: PlasmaCore.FrameSvg.LeftBorder | PlasmaCore.FrameSvg.BottomBorder
            }
        }
    }
}
