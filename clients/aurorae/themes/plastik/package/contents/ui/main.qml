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
import org.kde.kwin.decoration 0.1
import org.kde.kwin.decorations.plastik 1.0

Decoration {
    function readConfig() {
        switch (decoration.readConfig("BorderSize", DecorationOptions.BorderNormal)) {
        case DecorationOptions.BorderTiny:
            root.borderSize = 3;
            break;
        case DecorationOptions.BorderLarge:
            root.borderSize = 8;
            break;
        case DecorationOptions.BorderVeryLarge:
            root.borderSize = 12;
            break;
        case DecorationOptions.BorderHuge:
            root.borderSize = 18;
            break;
        case DecorationOptions.BorderVeryHuge:
            root.borderSize = 27;
            break;
        case DecorationOptions.BorderOversized:
            root.borderSize = 40;
            break;
        case DecorationOptions.BorderNormal: // fall through to default
        default:
            root.borderSize = 4;
            break;
        }
    }
    ColorHelper {
        id: colorHelper
    }
    DecorationOptions {
        id: options
        deco: decoration
    }
    property alias buttonSize: titleRow.captionHeight
    property color titleBarColor: options.titleBarColor
    property int animationDuration: 150
    Behavior on titleBarColor {
        ColorAnimation {
            duration: root.animationDuration
        }
    }
    property int borderSize: 4
    id: root
    borderLeft: borderSize
    borderRight: borderSize
    borderTop: top.normalHeight
    borderBottom: borderSize
    borderLeftMaximized: 0
    borderRightMaximized: 0
    borderBottomMaximized: 0
    borderTopMaximized: top.maximizedHeight
    paddingLeft: 0
    paddingRight: 0
    paddingBottom: 0
    paddingTop: 0
    Rectangle {
        color: root.titleBarColor
        anchors {
            fill: parent
        }
        border {
            width: decoration.maximized ? 0 : 2
            color: colorHelper.shade(root.titleBarColor, ColorHelper.DarkShade)
        }
        Rectangle {
            id: borderLeft
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                leftMargin: 1
                bottomMargin: 1
                topMargin: 1
            }
            visible: !decoration.maximized
            width: root.borderLeft
            color: root.titleBarColor
            Rectangle {
                width: 1
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                color: colorHelper.shade(root.titleBarColor, ColorHelper.LightShade, colorHelper.contrast - (decoration.active ? 0.4 : 0.8))
            }
        }
        Rectangle {
            id: borderRight
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
                rightMargin: 1
                bottomMargin: 1
                topMargin: 1
            }
            visible: !decoration.maximzied
            width: root.borderRight -1
            color: root.titleBarColor
            Rectangle {
                width: 1
                anchors {
                    bottom: parent.bottom
                    top: parent.top
                }
                x: parent.x + parent.width - 1
                color: colorHelper.shade(root.titleBarColor, ColorHelper.DarkShade, colorHelper.contrast - (decoration.active ? 0.4 : 0.8))
            }
        }
        Rectangle {
            id: borderBottom
            anchors {
                left: parent.right
                right: parent.left
                bottom: parent.bottom
                leftMargin: 1
                rightMargin: 1
            }
            height: root.borderBottom
            visible: !decoration.maximzied
            color: root.titleBarColor
            Rectangle {
                height: 1
                anchors {
                    left: parent.left
                    right: parent.right
                }
                y: parent.y + parent.height - 1
                color: colorHelper.shade(root.titleBarColor, ColorHelper.DarkShade, colorHelper.contrast - (decoration.active ? 0.4 : 0.8))
            }
        }

        Rectangle {
            id: top
            property int topMargin: 1
            property real normalHeight: titleRow.normalHeight + topMargin + 1
            property real maximizedHeight: titleRow.maximizedHeight
            height: decoration.maximized ? maximizedHeight : normalHeight
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                topMargin: decoration.maximized ? 0 : top.topMargin
                leftMargin: decoration.maximized ? 0 : 2
                rightMargin: decoration.maximized ? 0 : 2
            }
            gradient: Gradient {
                id: topGradient
                GradientStop {
                    position: 0.0
                    color: colorHelper.shade(root.titleBarColor, ColorHelper.MidlightShade, colorHelper.contrast - 0.4)
                }
                GradientStop {
                    id: middleGradientStop
                    position: 4.0/(decoration.maximized ? top.maximizedHeight : top.normalHeight)
                    color: colorHelper.shade(root.titleBarColor, ColorHelper.MidShade, colorHelper.contrast - 0.4)
                }
                GradientStop {
                    position: 1.0
                    color: root.titleBarColor
                }
            }
            Rectangle {
                height: 1
                anchors {
                    top: top.top
                    left: top.left
                    right: top.right
                }
                visible: !decoration.maximized
                color: colorHelper.shade(root.titleBarColor, ColorHelper.LightShade, colorHelper.contrast - (decoration.active ? 0.4 : 0.8))
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

            Item {
                id: titleRow
                property real captionHeight: caption.implicitHeight + 4
                property int topMargin: 4
                property int bottomMargin: 2
                property real normalHeight: captionHeight + bottomMargin + topMargin
                property real maximizedHeight: captionHeight + bottomMargin
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    topMargin: decoration.maximized ? 0 : titleRow.topMargin
                    leftMargin: decoration.maximized ? 0 : 6
                    rightMargin: decoration.maximized ? 0 : 6
                    bottomMargin: titleRow.bottomMargin
                }
                ButtonGroup {
                    id: leftButtonGroup
                    spacing: 1
                    explicitSpacer: root.buttonSize
                    menuButton: menuButtonComponent
                    minimizeButton: minimizeButtonComponent
                    maximizeButton: maximizeButtonComponent
                    keepBelowButton: keepBelowButtonComponent
                    keepAboveButton: keepAboveButtonComponent
                    helpButton: helpButtonComponent
                    shadeButton: shadeButtonComponent
                    allDesktopsButton: stickyButtonComponent
                    closeButton: closeButtonComponent
                    buttons: options.titleButtonsLeft
                    anchors {
                        top: parent.top
                        left: parent.left
                    }
                }
                Text {
                    id: caption
                    anchors {
                        top: parent.top
                        left: leftButtonGroup.right
                        right: rightButtonGroup.left
                        rightMargin: 5
                        leftMargin: 5
                    }
                    color: options.fontColor
                    Behavior on color {
                        ColorAnimation { duration: root.animationDuration }
                    }
                    text: decoration.caption
                    font: options.titleFont
                    style: Text.Raised
                    styleColor: colorHelper.shade(color, ColorHelper.ShadowShade)
                    elide: Text.ElideMiddle
                }
                ButtonGroup {
                    id: rightButtonGroup
                    spacing: 1
                    explicitSpacer: root.buttonSize
                    menuButton: menuButtonComponent
                    minimizeButton: minimizeButtonComponent
                    maximizeButton: maximizeButtonComponent
                    keepBelowButton: keepBelowButtonComponent
                    keepAboveButton: keepAboveButtonComponent
                    helpButton: helpButtonComponent
                    shadeButton: shadeButtonComponent
                    allDesktopsButton: stickyButtonComponent
                    closeButton: closeButtonComponent
                    buttons: options.titleButtonsRight
                    anchors {
                        top: parent.top
                        right: parent.right
                    }
                }
            }
        }

        Rectangle {
            id: innerBorder
            anchors {
                fill: parent
                leftMargin: root.borderLeft - 1
                rightMargin: root.borderRight
                topMargin: root.borderTop - 1
                bottomMargin: root.borderBottom
            }
            border {
                width: 1
                color: colorHelper.shade(root.titleBarColor, ColorHelper.MidShade)
            }
            color: root.titleBarColor
        }
    }

    Component {
        id: maximizeButtonComponent
        PlastikButton {
            buttonType: "A"
            size: root.buttonSize
        }
    }
    Component {
        id: keepBelowButtonComponent
        PlastikButton {
            buttonType: "B"
            size: root.buttonSize
        }
    }
    Component {
        id: keepAboveButtonComponent
        PlastikButton {
            buttonType: "F"
            size: root.buttonSize
        }
    }
    Component {
        id: helpButtonComponent
        PlastikButton {
            buttonType: "H"
            size: root.buttonSize
        }
    }
    Component {
        id: minimizeButtonComponent
        PlastikButton {
            buttonType: "I"
            size: root.buttonSize
        }
    }
    Component {
        id: shadeButtonComponent
        PlastikButton {
            buttonType: "L"
            size: root.buttonSize
        }
    }
    Component {
        id: stickyButtonComponent
        PlastikButton {
            buttonType: "S"
            size: root.buttonSize
        }
    }
    Component {
        id: closeButtonComponent
        PlastikButton {
            buttonType: "X"
            size: root.buttonSize
        }
    }
    Component {
        id: menuButtonComponent
        MenuButton {
            width: root.buttonSize
            height: root.buttonSize
        }
    }
    Component.onCompleted: readConfig()
    Connections {
        target: decoration
        onConfigChanged: readConfig()
    }
}
