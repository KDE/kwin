/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: MIT
*/

import QtQuick
import org.kde.kwin

SceneEffect {
    id: effect

    delegate: Rectangle {
        color: effect.configuration.BackgroundColor

        Text {
            anchors.centerIn: parent
            text: SceneView.screen.name
        }

        MouseArea {
            anchors.fill: parent
            onClicked: effect.visible = false
        }
    }

    ScreenEdgeHandler {
        enabled: true
        edge: ScreenEdgeHandler.TopEdge
        onActivated: effect.visible = !effect.visible
    }

    ShortcutHandler {
        name: "Toggle Quick Effect"
        text: "Toggle Quick Effect"
        sequence: "Meta+Ctrl+Q"
        onActivated: effect.visible = !effect.visible
    }

    PinchGestureHandler {
        direction: PinchGestureHandler.Direction.Contracting
        fingerCount: 3
        onActivated: effect.visible = !effect.visible
    }
}
