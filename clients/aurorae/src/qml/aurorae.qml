/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 1.1
import org.kde.plasma.core 0.1 as PlasmaCore

Decoration {
    function updateState() {
        if (!decoration.active && decoration.maximized && backgroundSvg.supportsMaximizedInactive) {
            root.state = "maximized-inactive";
        } else if (decoration.maximized && backgroundSvg.supportsMaximized) {
            root.state = "maximized";
        } else if (!decoration.active && backgroundSvg.supportsInactive) {
            root.state = "inactive";
        } else {
            root.state = "active";
        }
    }
    id: root
    borderLeft: Math.max(0, auroraeTheme.borderLeft)
    borderRight: Math.max(0, auroraeTheme.borderRight)
    borderTop: Math.max(0, auroraeTheme.borderTop)
    borderBottom: Math.max(0, auroraeTheme.borderBottom)
    borderLeftMaximized: Math.max(0, auroraeTheme.borderLeftMaximized)
    borderRightMaximized: Math.max(0, auroraeTheme.borderRightMaximized)
    borderBottomMaximized: Math.max(0, auroraeTheme.borderBottomMaximized)
    borderTopMaximized: Math.max(0, auroraeTheme.borderTopMaximized)
    paddingLeft: auroraeTheme.paddingLeft
    paddingRight: auroraeTheme.paddingRight
    paddingBottom: auroraeTheme.paddingBottom
    paddingTop: auroraeTheme.paddingTop
    Component.onCompleted: updateState()
    PlasmaCore.FrameSvg {
        property bool supportsInactive: hasElementPrefix("decoration-inactive")
        property bool supportsMaximized: hasElementPrefix("decoration-maximized")
        property bool supportsMaximizedInactive: hasElementPrefix("decoration-maximized-inactive")
        property bool supportsInnerBorderInactive: hasElementPrefix("innerborder-inactive")
        id: backgroundSvg
        imagePath: auroraeTheme.decorationPath
    }
    PlasmaCore.FrameSvgItem {
        id: decorationActive
        anchors.fill: parent
        imagePath: backgroundSvg.imagePath
        prefix: "decoration"
    }
    PlasmaCore.FrameSvgItem {
        id: decorationInactive
        anchors.fill: parent
        imagePath: backgroundSvg.imagePath
        prefix: "decoration-inactive"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: decorationMaximized
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: parent.paddingLeft
            rightMargin: parent.paddingRight
            topMargin: parent.paddingTop
        }
        imagePath: backgroundSvg.imagePath
        prefix: "decoration-maximized"
        opacity: 0
        height: parent.borderTopMaximized
    }
    PlasmaCore.FrameSvgItem {
        id: decorationMaximizedInactive
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: parent.paddingLeft
            rightMargin: parent.paddingRight
            topMargin: parent.paddingTop
        }
        imagePath: backgroundSvg.imagePath
        prefix: "decoration-maximized-inactive"
        opacity: 0
        height: parent.borderTopMaximized
    }
    AuroraeButtonGroup {
        id: leftButtonGroup
        buttons: options.customButtonPositions ? options.leftButtons : auroraeTheme.defaultButtonsLeft
        width: childrenRect.width
        anchors {
            top: parent.top
            left: parent.left
            leftMargin: auroraeTheme.titleEdgeLeft + root.paddingLeft
            topMargin: root.paddingTop + auroraeTheme.titleEdgeTop + auroraeTheme.buttonMarginTop
        }
        transitions: [
            Transition {
                to: "normal"
                ParallelAnimation {
                    NumberAnimation { target: leftButtonGroup; property: "anchors.leftMargin"; to: auroraeTheme.titleEdgeLeft + root.paddingLeft; duration: auroraeTheme.animationTime }
                    NumberAnimation { target: leftButtonGroup; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTop + auroraeTheme.buttonMarginTop; duration: auroraeTheme.animationTime }
                }
            },
            Transition {
                to: "maximized"
                ParallelAnimation {
                    NumberAnimation { target: leftButtonGroup; property: "anchors.leftMargin"; to: auroraeTheme.titleEdgeLeftMaximized + root.paddingLeft; duration: auroraeTheme.animationTime }
                    NumberAnimation { target: leftButtonGroup; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTopMaximized + auroraeTheme.buttonMarginTop; duration: auroraeTheme.animationTime }
                }
            }
        ]
    }
    AuroraeButtonGroup {
        id: rightButtonGroup
        buttons: options.customButtonPositions ? options.rightButtons : auroraeTheme.defaultButtonsRight
        width: childrenRect.width
        anchors {
            top: parent.top
            right: parent.right
            rightMargin: auroraeTheme.titleEdgeRight + root.paddingRight
            topMargin: root.paddingTop + auroraeTheme.titleEdgeTop + auroraeTheme.buttonMarginTop
        }
        transitions: [
            Transition {
                to: "normal"
                ParallelAnimation {
                    NumberAnimation { target: rightButtonGroup; property: "anchors.rightMargin"; to: auroraeTheme.titleEdgeRight + root.paddingRight; duration: auroraeTheme.animationTime }
                    NumberAnimation { target: rightButtonGroup; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTop + auroraeTheme.buttonMarginTop; duration: auroraeTheme.animationTime }
                }
            },
            Transition {
                to: "maximized"
                ParallelAnimation {
                    NumberAnimation { target: rightButtonGroup; property: "anchors.rightMargin"; to: auroraeTheme.titleEdgeRightMaximized + root.paddingRight; duration: auroraeTheme.animationTime }
                    NumberAnimation { target: rightButtonGroup; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTopMaximized + auroraeTheme.buttonMarginTop; duration: auroraeTheme.animationTime }
                }
            }
        ]
    }
    Text {
        id: caption
        text: decoration.caption
        horizontalAlignment: auroraeTheme.horizontalAlignment
        verticalAlignment: auroraeTheme.verticalAlignment
        elide: Text.ElideRight
        height: auroraeTheme.titleHeight
        anchors {
            left: leftButtonGroup.right
            right: rightButtonGroup.left
            top: root.top
            topMargin: root.paddingTop + auroraeTheme.titleEdgeTop
            leftMargin: auroraeTheme.titleBorderLeft
            rightMargin: auroraeTheme.titleBorderRight
        }
        MouseArea {
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            anchors.fill: parent
            onDoubleClicked: decoration.titlebarDblClickOperation()
            onPressed: decoration.titlePressed(mouse.button, mouse.buttons)
            onReleased: decoration.titleReleased(mouse.button, mouse.buttons)
        }
    }
    PlasmaCore.FrameSvgItem {
        function checkVisible() {
            if (decoration.maximized) {
                state = "invisible";
            } else {
                if (!decoration.active && backgroundSvg.supportsInnerBorderInactive) {
                    state = "invisible";
                } else {
                    state = "visible";
                }
            }
        }
        id: innerBorder
        anchors {
            fill: parent
            leftMargin: parent.paddingLeft + parent.borderLeft - margins.left
            rightMargin: parent.paddingRight + parent.borderRight - margins.right
            topMargin: parent.paddingTop + parent.borderTop - margins.top
            bottomMargin: parent.paddingBottom + parent.borderBottom - margins.bottom
        }
        imagePath: backgroundSvg.imagePath
        prefix: "innerborder"
        states: [
            State { name: "visible" },
            State { name: "invisible" }
        ]
        transitions:  [
            Transition {
                to: "visible"
                ParallelAnimation {
                    NumberAnimation { target: innerBorder; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                }
            },
            Transition {
                to: "invisible"
                ParallelAnimation {
                    NumberAnimation { target: innerBorder; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                }
            }
        ]
        Component.onCompleted: checkVisible()
        Connections {
            target: decoration
            onActiveChanged: innerBorder.checkVisible()
            onMaximizedChanged: innerBorder.checkVisible()
        }
    }
    PlasmaCore.FrameSvgItem {
        function checkVisible() {
            if (decoration.maximized) {
                state = "invisible";
            } else {
                if (!decoration.active && backgroundSvg.supportsInnerBorderInactive) {
                    state = "visible";
                } else {
                    state = "invisible";
                }
            }
        }
        id: innerBorderInactive
        anchors {
            fill: parent
            leftMargin: parent.paddingLeft + parent.borderLeft - margins.left
            rightMargin: parent.paddingRight + parent.borderRight - margins.right
            topMargin: parent.paddingTop + parent.borderTop - margins.top
            bottomMargin: parent.paddingBottom + parent.borderBottom - margins.bottom
        }
        imagePath: backgroundSvg.imagePath
        prefix: "innerborder-inactive"
        states: [
            State { name: "visible" },
            State { name: "invisible" }
        ]
        transitions:  [
            Transition {
                to: "visible"
                ParallelAnimation {
                    NumberAnimation { target: innerBorderInactive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                }
            },
            Transition {
                to: "invisible"
                ParallelAnimation {
                    NumberAnimation { target: innerBorderInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                }
            }
        ]
        Component.onCompleted: checkVisible()
        Connections {
            target: decoration
            onActiveChanged: innerBorderInactive.checkVisible()
            onMaximizedChanged: innerBorderInactive.checkVisible()
        }
    }
    states: [
        State { name: "active" },
        State { name: "inactive" },
        State { name: "maximized" },
        State { name: "maximized-inactive" }
    ]
    transitions: [
        Transition {
            to: "active"
            // Cross fade from inactive to active
            ParallelAnimation {
                NumberAnimation { target: decorationActive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximized; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                ColorAnimation { target: caption; property: "color"; to: auroraeTheme.activeTextColor; duration: auroraeTheme.animationTime }
                NumberAnimation { target: caption; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTop; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "inactive"
            // Cross fade from active to inactive
            ParallelAnimation {
                NumberAnimation { target: decorationActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationInactive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximized; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximizedInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                ColorAnimation { target: caption; property: "color"; to: auroraeTheme.inactiveTextColor; duration: auroraeTheme.animationTime }
                NumberAnimation { target: caption; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTop; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "maximized"
            ParallelAnimation {
                NumberAnimation { target: decorationActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximizedInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximized; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                ColorAnimation { target: caption; property: "color"; to: auroraeTheme.activeTextColor; duration: auroraeTheme.animationTime }
                NumberAnimation { target: caption; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTopMaximized; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "maximized-inactive"
            ParallelAnimation {
                NumberAnimation { target: decorationActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximizedInactive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationMaximized; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                ColorAnimation { target: caption; property: "color"; to: auroraeTheme.inactiveTextColor; duration: auroraeTheme.animationTime }
                NumberAnimation { target: caption; property: "anchors.topMargin"; to: root.paddingTop + auroraeTheme.titleEdgeTopMaximized; duration: auroraeTheme.animationTime }
            }
        }
    ]
    Connections {
        target: decoration
        onActiveChanged: updateState()
        onMaximizedChanged: updateState()
    }
}
