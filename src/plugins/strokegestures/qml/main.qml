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

    readonly property bool hasAlphaChannel: true // for QuickSceneView

    readonly property bool strokeStartedOnTargetScreen: strokeShape.contains(Qt.point(strokeShape.startX, strokeShape.startY))

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
                effect.deactivate(effect.strokeFadeOutMsec);
            }
            function onStrokeCancelled() {
                effect.deactivate(effect.strokeFadeOutMsec);
            }
        }

        opacity: effect.active ? 1 : 0
        Behavior on opacity {
            OpacityAnimator {
                duration: effect.strokeFadeOutMsec
                easing.type: Easing.OutCubic
            }
        }
    }
}
