/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property QtObject effect
    color: Qt.rgba(1.0, 1.0, 1.0, 0.5)

    ColumnLayout {
        anchors.fill: parent

        Text {
            color: root.effect.paintColor
            text: root.effect.fps + "/" + root.effect.maximumFps
        }

        Text {
            Layout.fillWidth: true
            text: i18nc("@label", "This effect is not a benchmark")
        }
    }
}
