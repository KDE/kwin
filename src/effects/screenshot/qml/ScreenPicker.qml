/* SPDX-FileCopyrightText: 2023 Noah Davis <noahadvs@gmail.com>
 */

import QtQuick 2.15
import QtQml 2.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.core 2.0 as PlasmaCore

MouseArea {
    id: root

    required property QtObject effect
    property bool selected: false

    hoverEnabled: true
    cursorShape: Qt.CrossCursor

    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: opacity > 0
        opacity: if (selected || root.containsPress) {
            return 0
        } else if (root.containsMouse) {
            return 0.25
        } else {
            return 0.5
        }
        Behavior on opacity {
            OpacityAnimator {
                duration: PlasmaCore.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        color: "transparent"
        anchors.fill: parent
        border.width: 2
        border.color: PlasmaCore.ColorScope.highlightColor
        visible: opacity > 0
        opacity: root.containsMouse
        Behavior on opacity {
            OpacityAnimator {
                duration: PlasmaCore.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    onClicked: root.selected = !root.selected
}
