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
    required property QtObject targetScreen
    required property var windowRects

    activeFocusOnTab: false

    function rectContains(rect: rect, x: real, y: real) {
        return rect.left <= x
            && rect.top <= y
            && rect.right >= x
            && rect.bottom >= y
    }

    hoverEnabled: true
    cursorShape: Qt.CrossCursor

    Instantiator {
        model: root.windowRects
        delegate: Rectangle {
            readonly property rect windowRect: modelData
            readonly property bool containsMouse: contains(Qt.point(root.mouseX, root.mouseY))
            property bool selected: false
            activeFocusOnTab: true
            focus: index === 0
            parent: root
            x: windowRect.x - root.targetScreen.geometry.x
            y: windowRect.y - root.targetScreen.geometry.y
            width: windowRect.width
            height: windowRect.height
            color: "transparent"
            border.width: 1
            border.color: PlasmaCore.ColorScope.highlightColor
            visible: opacity > 0
            opacity: if (selected) {
                return 1
            } else if (containsMouse) {
                return 0.5
            } else {
                return 0
            }
            Behavior on opacity {
                OpacityAnimator {
                    duration: PlasmaCore.Units.longDuration
                    easing.type: Easing.OutCubic
                }
            }

            Rectangle {
                z: -1
                anchors.fill: parent
                anchors.margins: -border.width
                visible: opacity > 0
                opacity: parent.activeFocus ? 0.33 : 0
                Behavior on opacity {
                    OpacityAnimator {
                        duration: PlasmaCore.Units.longDuration
                        easing.type: Easing.OutCubic
                    }
                }
                color: "transparent"
                border.width: 2
                border.color: PlasmaCore.ColorScope.highlightColor
            }
        }
    }

    onClicked: mouse => {
        let item = childAt(mouse.x, mouse.y)
        if (item !== null && item.containsMouse) {
            item.selected = !item.selected
        }
    }
}
