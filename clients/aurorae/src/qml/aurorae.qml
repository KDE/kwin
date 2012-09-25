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
    PlasmaCore.FrameSvg {
        property bool supportsInactive: hasElementPrefix("decoration-inactive")
        property bool supportsMaximized: hasElementPrefix("decoration-maximized")
        property bool supportsMaximizedInactive: hasElementPrefix("decoration-maximized-inactive")
        property bool supportsInnerBorder: hasElementPrefix("innerborder")
        property bool supportsInnerBorderInactive: hasElementPrefix("innerborder-inactive")
        id: backgroundSvg
        imagePath: auroraeTheme.decorationPath
    }
    PlasmaCore.FrameSvgItem {
        id: decorationActive
        property bool shown: (!decoration.maxized || !backgroundSvg.supportsMaximized) && (decoration.active || !backgroundSvg.supportsInactive)
        anchors.fill: parent
        imagePath: backgroundSvg.imagePath
        prefix: "decoration"
        opacity: shown ? 1 : 0
        enabledBorders: decoration.maximized ? PlasmaCore.FrameSvg.NoBorder : PlasmaCore.FrameSvg.TopBorder | PlasmaCore.FrameSvg.BottomBorder | PlasmaCore.FrameSvg.LeftBorder | PlasmaCore.FrameSvg.RightBorder
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: decorationInactive
        anchors.fill: parent
        imagePath: backgroundSvg.imagePath
        prefix: "decoration-inactive"
        opacity: (!decoration.active && backgroundSvg.supportsInactive) ? 1 : 0
        enabledBorders: decoration.maximized ? PlasmaCore.FrameSvg.NoBorder : PlasmaCore.FrameSvg.TopBorder | PlasmaCore.FrameSvg.BottomBorder | PlasmaCore.FrameSvg.LeftBorder | PlasmaCore.FrameSvg.RightBorder
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: decorationMaximized
        property bool shown: decoration.maximized && backgroundSvg.supportsMaximized && (decoration.active || !backgroundSvg.supportsMaximizedInactive)
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
        height: parent.borderTopMaximized
        opacity: shown ? 1 : 0
        enabledBorders: PlasmaCore.FrameSvg.NoBorder
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
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
        height: parent.borderTopMaximized
        opacity: (!decoration.active && decoration.maximized && backgroundSvg.supportsMaximizedInactive) ? 1 : 0
        enabledBorders: PlasmaCore.FrameSvg.NoBorder
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    AuroraeButtonGroup {
        id: leftButtonGroup
        buttons: options.customButtonPositions ? options.leftButtons : auroraeTheme.defaultButtonsLeft
        width: childrenRect.width
        anchors {
            left: parent.left
            leftMargin: decoration.maximized ? auroraeTheme.titleEdgeLeftMaximized : (auroraeTheme.titleEdgeLeft + root.paddingLeft)
        }
        Behavior on anchors.leftMargin {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    AuroraeButtonGroup {
        id: rightButtonGroup
        buttons: options.customButtonPositions ? options.rightButtons : auroraeTheme.defaultButtonsRight
        width: childrenRect.width
        anchors {
            right: parent.right
            rightMargin: decoration.maximized ? auroraeTheme.titleEdgeRightMaximized : (auroraeTheme.titleEdgeRight + root.paddingRight)
        }
        Behavior on anchors.rightMargin {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    Text {
        id: caption
        text: decoration.caption
        textFormat: Text.PlainText
        horizontalAlignment: auroraeTheme.horizontalAlignment
        verticalAlignment: auroraeTheme.verticalAlignment
        elide: Text.ElideRight
        height: Math.max(auroraeTheme.titleHeight, auroraeTheme.buttonHeight * auroraeTheme.buttonSizeFactor)
        color: decoration.active ? auroraeTheme.activeTextColor : auroraeTheme.inactiveTextColor
        font: decoration.active ? options.activeTitleFont : options.inactiveTitleFont
        anchors {
            left: leftButtonGroup.right
            right: rightButtonGroup.left
            top: root.top
            topMargin: decoration.maximized ? auroraeTheme.titleEdgeTopMaximized : (auroraeTheme.titleEdgeTop + root.paddingTop)
            leftMargin: auroraeTheme.titleBorderLeft
            rightMargin: auroraeTheme.titleBorderRight
        }
        MouseArea {
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            anchors.fill: parent
            onDoubleClicked: decoration.titlebarDblClickOperation()
            onPressed: {
                if (mouse.button == Qt.LeftButton) {
                    mouse.accepted = false;
                } else {
                    decoration.titlePressed(mouse.button, mouse.buttons);
                }
            }
            onReleased: decoration.titleReleased(mouse.button, mouse.buttons)
        }
        Behavior on color {
            ColorAnimation {
                duration: auroraeTheme.animationTime
            }
        }
        Behavior on anchors.topMargin {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
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
        opacity: (decoration.active && !decoration.maximized && backgroundSvg.supportsInnerBorder) ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
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
        opacity: (!decoration.active && !decoration.maximized && backgroundSvg.supportsInnerBorderInactive) ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
}
