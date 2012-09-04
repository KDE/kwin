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

DecorationButton {
    id: button
    function colorize() {
        var highlightColor = null;
        if (button.pressed) {
            if (button.buttonType == "X") {
                highlightColor = colorHelper.foreground(decoration.active, ColorHelper.NegativeText);
            } else {
                highlightColor = options.titleBarColor;
            }
            highlightColor = colorHelper.shade(highlightColor, ColorHelper.ShadowShade);
            highlightColor = colorHelper.multiplyAlpha(highlightColor, 0.3);
        } else if (button.hovered) {
            if (button.buttonType == "X") {
                highlightColor = colorHelper.foreground(decoration.active, ColorHelper.NegativeText);
            } else {
                highlightColor = options.titleBarColor;
            }
            highlightColor = colorHelper.shade(highlightColor, ColorHelper.LightShade, Math.min(1.0, colorHelper.contrast + 0.4));
            highlightColor = colorHelper.multiplyAlpha(highlightColor, 0.6);
        }
        if (highlightColor) {
            button.surfaceTop = Qt.tint(button.baseSurfaceTop, highlightColor);
            button.surfaceBottom = Qt.tint(button.baseSurfaceBottom, highlightColor);
            highlightColor = colorHelper.multiplyAlpha(highlightColor, 0.4);
            button.conturTop = Qt.tint(button.baseConturTop, highlightColor);
            button.conturBottom = Qt.tint(button.baseConturBottom, highlightColor);
        } else {
            button.conturTop = button.baseConturTop;
            button.conturBottom = button.baseConturBottom;
            button.surfaceTop = button.baseSurfaceTop;
            button.surfaceBottom = button.baseSurfaceBottom;
        }
    }
    property real size
    property color conturTop
    property color conturBottom
    property color surfaceTop
    property color surfaceBottom
    property color baseConturTop: colorHelper.shade(options.titleBarColor, ColorHelper.DarkShade, colorHelper.contrast - 0.4)
    property color baseConturBottom: colorHelper.shade(options.titleBarColor, ColorHelper.MidShade)
    property color baseSurfaceTop: colorHelper.shade(options.titleBarColor, ColorHelper.MidlightShade, colorHelper.contrast - 0.4)
    property color baseSurfaceBottom: colorHelper.shade(options.titleBarColor, ColorHelper.LightShade, colorHelper.contrast - 0.4)
    Behavior on conturTop {
        ColorAnimation { duration: root.animateButtons ? root.animationDuration : 0 }
    }
    Behavior on conturBottom {
        ColorAnimation { duration: root.animateButtons ? root.animationDuration : 0 }
    }
    Behavior on surfaceTop {
        ColorAnimation { duration: root.animateButtons ? root.animationDuration : 0 }
    }
    Behavior on surfaceBottom {
        ColorAnimation { duration: root.animateButtons ? root.animationDuration : 0 }
    }
    width: size
    height: size
    Rectangle {
        radius: 2
        smooth: true
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: button.conturTop
            }
            GradientStop {
                position: 1.0
                color: button.conturBottom
            }
        }
        Rectangle {
            radius: 2
            smooth: true
            anchors {
                fill: parent
                leftMargin: 1
                rightMargin: 1
                topMargin: 1
                bottomMargin: 1
            }
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: button.surfaceTop
                }
                GradientStop {
                    position: 1.0
                    color: button.surfaceBottom
                }
            }
        }
    }
    Item {
        property int imageWidth: button.width > 14 ? button.width - 2 * Math.floor(button.width/3.5) : button.width - 6
        property int imageHeight: button.height > 14 ? button.height - 2 * Math.floor(button.height/3.5) : button.height - 6
        property string source: "image://plastik/" + button.buttonType + "/" + decoration.active + "/" + ((buttonType == "A") ? decoration.maximized : button.toggled)
        anchors.fill: parent
        Image {
            id: shadowImage
            x: button.x + button.width / 2 - width / 2 + 1
            y: button.y + button.height / 2 - height / 2 + 1
            source: parent.source + "/true"
            width: parent.imageWidth
            height: parent.imageHeight
            sourceSize.width: width
            sourceSize.height: height
            visible: !button.pressed
        }
        Image {
            id: image
            x: button.x + button.width / 2 - width / 2
            y: button.y + button.height / 2 - height / 2 + (button.pressed ? 1 : 0)
            source: parent.source
            width: parent.imageWidth
            height: parent.imageHeight
            sourceSize.width: width
            sourceSize.height: height
        }
    }
    Component.onCompleted: {
        colorize();
        if (buttonType == "H") {
            visible = decoration.providesContextHelp;
        }
    }
    onHoveredChanged: colorize()
    onPressedChanged: colorize()
    Connections {
        target: decoration
        onActiveChanged: button.colorize()
    }
}
