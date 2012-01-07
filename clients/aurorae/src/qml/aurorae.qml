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
    id: root
    borderLeft: auroraeTheme.borderLeft
    borderRight: auroraeTheme.borderRight
    borderTop: auroraeTheme.borderTop
    borderBottom: auroraeTheme.borderBottom
    borderLeftMaximized: auroraeTheme.borderLeftMaximized
    borderRightMaximized: auroraeTheme.borderRightMaximized
    borderBottomMaximized: auroraeTheme.borderBottomMaximized
    borderTopMaximized: auroraeTheme.borderTopMaximized
    paddingLeft: auroraeTheme.paddingLeft
    paddingRight: auroraeTheme.paddingRight
    paddingBottom: auroraeTheme.paddingBottom
    paddingTop: auroraeTheme.paddingTop
    Component.onCompleted: {
        if (decoration.active) {
            root.state = "active";
        } else {
            if (backgroundSvg.supportsInactive) {
                root.state = "inactive";
            } else {
                root.state = "active";
            }
        }
    }
    PlasmaCore.FrameSvg {
        property bool supportsInactive: hasElementPrefix("decoration-inactive")
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
    AuroraeButtonGroup {
        id: leftButtonGroup
        buttons: decoration.leftButtons
        width: childrenRect.width
        anchors {
            top: parent.top
            left: parent.left
            leftMargin: auroraeTheme.titleEdgeLeft + root.paddingLeft
            topMargin: root.paddingTop + auroraeTheme.titleEdgeTop + auroraeTheme.buttonMarginTop
        }
    }
    AuroraeButtonGroup {
        id: rightButtonGroup
        buttons: decoration.rightButtons
        width: childrenRect.width
        anchors {
            top: parent.top
            right: parent.right
            rightMargin: auroraeTheme.titleEdgeRight + root.paddingRight
            topMargin: root.paddingTop + auroraeTheme.titleEdgeTop + auroraeTheme.buttonMarginTop
        }
    }
    Text {
        id: caption
        text: decoration.caption
        horizontalAlignment: auroraeTheme.horizontalAlignment
        verticalAlignment: auroraeTheme.verticalAlignment
        elide: Text.ElideRight
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
    states: [
        State { name: "active" },
        State { name: "inactive" }
    ]
    transitions: [
        Transition {
            to: "active"
            // Cross fade from inactive to active
            ParallelAnimation {
                NumberAnimation { target: decorationActive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                ColorAnimation { target: caption; property: "color"; to: auroraeTheme.activeTextColor; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "inactive"
            // Cross fade from active to inactive
            ParallelAnimation {
                NumberAnimation { target: decorationActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: decorationInactive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                ColorAnimation { target: caption; property: "color"; to: auroraeTheme.inactiveTextColor; duration: auroraeTheme.animationTime }
            }
        }
    ]
    Connections {
        target: decoration
        onActiveChanged: {
            if (decoration.active) {
                root.state = "active";
            } else {
                if (backgroundSvg.supportsInactive) {
                    root.state = "inactive";
                }
            }
        }
    }
}
