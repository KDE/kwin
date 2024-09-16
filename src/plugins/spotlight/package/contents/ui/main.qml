/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: MIT
*/

import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents

Window {
    id: root

    readonly property bool _q_showWithoutActivating: true

    color: "transparent"
    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    width: Screen.width * 2
    height: Screen.height * 2

    x: KWinComponents.Workspace.cursorPos.x - root.width / 2
    y: KWinComponents.Workspace.cursorPos.y - root.height / 2

    property int radius: 100

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            
            ctx.clearRect(0, 0, canvas.width, canvas.height)

            ctx.fillStyle = "rgba(0, 0, 0, 0.5)"
            ctx.fillRect(0, 0, canvas.width, canvas.height)

            ctx.globalCompositeOperation = "copy"
            ctx.fillStyle = "rgba(0, 0, 0, 0)"
            ctx.ellipse(canvas.width / 2 - root.radius, canvas.height / 2 - root.radius, root.radius * 2, root.radius * 2)
            ctx.fill()
        }
    }

    KWinComponents.ShortcutHandler {
        name: "Toggle Spotlight"
        text: "Toggle Spotlight"
        sequence: "Meta+Ctrl+L"
        onActivated: root.visible = !root.visible
    }

    KWinComponents.ShortcutHandler {
        name: "Increase Spotlight Size"
        text: "Increase Spotlight Size"
        sequence: "Meta+Ctrl+="
        onActivated: {
            root.radius *= 1.2
            console.log(root.radius)
            canvas.requestPaint()
        }
    }

    KWinComponents.ShortcutHandler {
        name: "Decrease Spotlight Size"
        text: "Decrease Spotlight Size"
        sequence: "Meta+Ctrl+-"
        onActivated: {
            root.radius /= 1.2
            console.log(root.radius)
            canvas.requestPaint()
        }
    }
}
