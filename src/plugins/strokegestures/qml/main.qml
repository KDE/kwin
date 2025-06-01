/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects

Item {
    id: root

    required property QtObject effect
    required property QtObject targetScreen

    readonly property bool strokeStartedOnTargetScreen: strokeShape.contains(Qt.point(strokeShape.startX, strokeShape.startY))

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
                    duration: effect.animationDurationMsec
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
                duration: effect.animationDurationMsec
                easing.type: Easing.OutCubic
            }
        }
    }

    StrokeShape {
        id: strokeShape
        anchors.fill: parent

        startX: -1
        startY: -1

        Connections {
            target: root.effect

            function onActiveChanged() {
                strokeShape.clearPoints();
                strokeShape.startX = -1;
                strokeShape.startY = -1;
            }
            function onStrokeStarted(initial: point) {
                const p = targetScreen.mapFromGlobal(initial);
                strokeShape.startX = p.x;
                strokeShape.startY = p.y;
            }
            function onStrokePointAdded(latest: point) {
                if (root.strokeStartedOnTargetScreen) {
                    strokeShape.addPoint(targetScreen.mapFromGlobal(latest));
                }
            }
            function onStrokePointReplaced(latest: point) {
                if (root.strokeStartedOnTargetScreen) {
                    strokeShape.replaceLatestPoint(targetScreen.mapFromGlobal(latest));
                }
            }
            function onStrokeEnded() {
                effect.deactivate(effect.animationDurationMsec);
            }
            function onStrokeCancelled() {
                effect.deactivate(effect.animationDurationMsec);
            }
        }

        opacity: effect.active ? 1 : 0
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.animationDurationMsec
                easing.type: Easing.OutCubic
            }
        }
    }
}
