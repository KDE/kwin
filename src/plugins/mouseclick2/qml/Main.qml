/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents
import org.kde.kitemmodels as KitemModels

Rectangle {
    id: root
}
/*
    readonly property QtObject effect: KWinComponents.SceneView.effect
    readonly property QtObject targetScreen: KWinComponents.SceneView.screen

    KWinComponents.DesktopBackground {
        id: backgroundItem
        anchors.fill: parent
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentDesktop
        outputName: targetScreen.name
    }
    Rectangle {
        width: 200
        height: 200
        color: "red"

    }

     ShaderEffectSource {
        id: source
        anchors.fill: parent
        sourceItem: backgroundItem
    }

    Item {
        id: ballContainer
        anchors.fill: parent
        Rectangle {
            id: ball1
            color: "red"
            width: 32
            height: 32
            radius: width/2
            x: point1.x - radius
            y: point1.y - radius
            visible: point1.pressed
        }
        Rectangle {
            id: ball2
            color: "red"
            width: 32
            height: 32
            radius: width/2
            x: point2.x - radius
            y: point2.y - radius
            visible: point2.pressed
        }
        Rectangle {
            id: ball3
            color: "red"
            width: 32
            height: 32
            radius: width/2
            x: point3.x - radius
            y: point3.y - radius
            visible: point3.pressed
        }

        Rectangle {
            id: cursor
            color: "red"
            width: 32
            height: 32
            radius: width/2
        }
    }

    ShaderEffectSource {
        id: ballSource
        sourceItem: ballContainer
        live: true
        hideSource: true
    }

    ShaderEffect {
        id: waterSimulation
        anchors.fill: parent

        property real damp: 0.9
        property real phase: 0.2
        property variant currentInput: ballSource
        property variant previousFrame: feedbackBuffer

       // property variant src: simulationSource
        property variant size: Qt.size(width, height)
        property variant point: Qt.point(0, 0)
        fragmentShader: "shaders/watersimulation.frag.qsb"
        vertexShader: "shaders/water.vert.qsb"
    }


    // 2. Render the generator to a texture
    ShaderEffectSource {
        id: feedbackBuffer
        sourceItem: waterSimulation
        anchors.fill: parent
        live: true
        hideSource: true
        recursive: true
        visible: false
    }

    ShaderEffect {
        anchors.fill: parent
        property variant simulation: feedbackBuffer
        property variant src: source
        property variant color: Kirigami.Theme.highlightColor
        fragmentShader: "shaders/water.frag.qsb"
        vertexShader: "shaders/water.vert.qsb"
    }

    Rectangle {
        color: "blue"
        opacity: point1.pressed * 0.3
        width: 32
        height: 32
        radius: width/2
        x: ball1.x
        y: ball1.y
        Behavior on opacity {
            NumberAnimation {
                duration: 250
                easing.type: Easing.InOutQuad
            }
        }
    }
    Rectangle {
        color: "red"
        opacity: point2.pressed * 0.3
        width: 32
        height: 32
        radius: width/2
        x: ball2.x
        y: ball2.y
        Behavior on opacity {
            NumberAnimation {
                duration: 250
                easing.type: Easing.InOutQuad
            }
        }
    }
    Rectangle {
        color: "green"
        opacity: point3.pressed * 0.3
        width: 32
        height: 32
        radius: width/2
        x: ball3.x
        y: ball3.y
        Behavior on opacity {
            NumberAnimation {
                duration: 250
                easing.type: Easing.InOutQuad
            }
        }
    }
    MultiPointTouchArea {
        id: multiPoint
        anchors.fill: parent
        minimumTouchPoints: 1
        maximumTouchPoints: 3
        mouseEnabled: true
        touchPoints: [
            TouchPoint { id: point1 },
            TouchPoint { id: point2 },
            TouchPoint { id: point3 }
        ]
    }

    MouseArea {
        id: mouse
       // visible: false
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: mouse => {
            let modString = "";
            if (mouse.modifiers != Qt.NoModifier) {
                if (mouse.modifiers & Qt.ShiftModifier) {
                    modString += "Shift + ";
                }
                if (mouse.modifiers & Qt.ControlModifier) {
                    modString += "Control + ";
                }
                if (mouse.modifiers & Qt.AltModifier) {
                    modString += "Alt + ";
                }
                if (mouse.modifiers & Qt.MetaModifier) {
                    modString += "Meta + ";
                }
            }
            let btnString = "";
            switch (mouse.button) {
            case Qt.LeftButton:
                btnString = "Left Button";
                break;
            case Qt.RightButton:
                btnString = "Right Button";
                break;
            case Qt.MiddleButton:
                btnString = "Middle Button";
                break;
            }
           // overlay.showMessage(modString + btnString)
        }
        onPressed: {
            cursor.x = mouse.x
            cursor.y = mouse.y
            cursor.visible = true
            print(cursor.x)
        }
        onPositionChanged: {
            cursor.x = mouse.x
            cursor.y = mouse.y
            print(cursor.x)
        }
        onReleased: {
            cursor.visible = false
        }
    }
}
*/
