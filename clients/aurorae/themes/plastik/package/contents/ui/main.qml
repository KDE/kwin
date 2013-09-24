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
            borders.setBorders(3);
            extendedBorders.setAllBorders(0);
            break;
        case DecorationOptions.BorderLarge:
            borders.setBorders(8);
            extendedBorders.setAllBorders(0);
            break;
        case DecorationOptions.BorderVeryLarge:
            borders.setBorders(12);
            extendedBorders.setAllBorders(0);
            break;
        case DecorationOptions.BorderHuge:
            borders.setBorders(18);
            extendedBorders.setAllBorders(0);
            break;
        case DecorationOptions.BorderVeryHuge:
            borders.setBorders(27);
            extendedBorders.setAllBorders(0);
            break;
        case DecorationOptions.BorderOversized:
            borders.setBorders(40);
            extendedBorders.setAllBorders(0);
            break;
        case DecorationOptions.BorderNoSides:
            borders.setBorders(4);
            borders.setSideBorders(1);
            extendedBorders.setSideBorders(3);
            break;
        case DecorationOptions.BorderNone:
            borders.setBorders(1);
            extendedBorders.setBorders(3);
            break;
        case DecorationOptions.BorderNormal: // fall through to default
        default:
            borders.setBorders(4);
            extendedBorders.setAllBorders(0);
            break;
        }
        var titleAlignLeft = decoration.readConfig("titleAlignLeft", true);
        var titleAlignCenter = decoration.readConfig("titleAlignCenter", false);
        var titleAlignRight = decoration.readConfig("titleAlignRight", false);
        if (titleAlignRight) {
            root.titleAlignment = Text.AlignRight;
        } else if (titleAlignCenter) {
            root.titleAlignment = Text.AlignHCenter;
        } else {
            if (!titleAlignLeft) {
                console.log("Error reading title alignment: all alignment options are false");
            }
            root.titleAlignment = Text.AlignLeft;
        }
        root.animateButtons = decoration.readConfig("animateButtons", true);
        root.titleShadow = decoration.readConfig("titleShadow", true);
        if (decoration.animationsSupported) {
            root.animationDuration = 150;
            root.animateButtons = false;
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
    property alias titleAlignment: caption.horizontalAlignment
    property color titleBarColor: options.titleBarColor
    // set by readConfig after Component completed, ensures that buttons do not flicker
    property int animationDuration: 0
    property bool animateButtons: true
    property bool titleShadow: true
    Behavior on titleBarColor {
        ColorAnimation {
            duration: root.animationDuration
        }
    }
    id: root
    alpha: false
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
            width: root.borders.left
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
            width: root.borders.right -1
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
            height: root.borders.bottom
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
            property real maximizedHeight: titleRow.maximizedHeight + 1
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
                property int topMargin: 3
                property int bottomMargin: 1
                property real normalHeight: captionHeight + bottomMargin + topMargin
                property real maximizedHeight: captionHeight + bottomMargin
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    topMargin: decoration.maximized ? 0 : titleRow.topMargin
                    leftMargin: decoration.maximized ? 0 : 3
                    rightMargin: decoration.maximized ? 0 : 3
                    bottomMargin: titleRow.bottomMargin
                }
                ButtonGroup {
                    id: leftButtonGroup
                    spacing: 1
                    explicitSpacer: root.buttonSize
                    menuButton: menuButtonComponent
                    appMenuButton: appMenuButtonComponent
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
                    textFormat: Text.PlainText
                    anchors {
                        top: parent.top
                        left: leftButtonGroup.right
                        right: rightButtonGroup.left
                        rightMargin: 5
                        leftMargin: 5
                        topMargin: 3
                    }
                    color: options.fontColor
                    Behavior on color {
                        ColorAnimation { duration: root.animationDuration }
                    }
                    text: decoration.caption
                    font: options.titleFont
                    style: root.titleShadow ? Text.Raised : Text.Normal
                    styleColor: colorHelper.shade(color, ColorHelper.ShadowShade)
                    elide: Text.ElideMiddle
                }
                ButtonGroup {
                    id: rightButtonGroup
                    spacing: 1
                    explicitSpacer: root.buttonSize
                    menuButton: menuButtonComponent
                    appMenuButton: appMenuButtonComponent
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

        Item {
            id: innerBorder
            anchors.fill: parent

            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                }
                height: 1
                y: top.height - 1
                visible: decoration.maximized
                color: colorHelper.shade(root.titleBarColor, ColorHelper.MidShade)
            }

            Rectangle {
                anchors {
                    fill: parent
                    leftMargin: root.borders.left - 1
                    rightMargin: root.borders.right
                    topMargin: root.borders.top - 1
                    bottomMargin: root.borders.bottom
                }
                border {
                    width: 1
                    color: colorHelper.shade(root.titleBarColor, ColorHelper.MidShade)
                }
                visible: !decoration.maximized
                color: root.titleBarColor
            }
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
    Component {
        id: appMenuButtonComponent
        PlastikButton {
            buttonType: "N"
            size: root.buttonSize
        }
    }
    Component.onCompleted: {
        borders.setBorders(4);
        borders.setTitle(top.normalHeight);
        maximizedBorders.setTitle(top.maximizedHeight);
        readConfig();
    }
    Connections {
        target: decoration
        onConfigChanged: readConfig()
    }
}
