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

Item {
    id: button
    property string buttonType : ""
    property bool hovered: false
    property bool pressed: false
    property bool toggled: false
    enabled: {
        switch (button.buttonType) {
        case "X":
            return decoration.closeable;
        case "A":
        case "R":
            return decoration.maximizeable;
        case "I":
            return decoration.minimizeable;
        case "_":
            return false;
        default:
            return true;
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: {
            switch (button.buttonType) {
            case "M":
                return Qt.LeftButton | Qt.RightButton;
            case "A":
            case "R":
                return Qt.LeftButton | Qt.RightButton | Qt.MiddleButton;
            default:
                return Qt.LeftButton;
            }
        }
        hoverEnabled: true
        onEntered: button.hovered = true
        onPositionChanged: decoration.titleMouseMoved(mouse.button, mouse.buttons)
        onExited: button.hovered = false
        onPressed: button.pressed = true
        onReleased: button.pressed = false
        onClicked: {
            switch (button.buttonType) {
            case "M":
                // menu
                decoration.menuClicked();
                break;
            case "S":
                // all desktops
                decoration.toggleOnAllDesktops();
                break;
            case "H":
                // help
                decoration.showContextHelp();
                break;
            case "I":
                // minimize
                decoration.minimize();
                break;
            case "A":
            case "R":
                // maximize
                decoration.maximize(mouse.button);
                break;
            case "X":
                // close
                decoration.closeWindow();
                break;
            case "F":
                // keep above
                decoration.toggleKeepAbove();
                break;
            case "B":
                // keep below
                decoration.toggleKeepBelow();
                break;
            case "L":
                // shade
                decoration.toggleShade();
                break;
            }
        }
        onDoubleClicked: {
            if (button.buttonType == "M") {
                decoration.closeWindow();
            }
        }
        Component.onCompleted: {
            switch (button.buttonType) {
            case "S":
                // all desktops
                button.toggled = decoration.onAllDesktops;
                break;
            case "F":
                button.toggled = decoration.keepAbove;
                break;
            case "B":
                button.toggled = decoration.keepBelow;
                break;
            case "L":
                button.toggled = decoration.shade;
                break;
            }
        }
        Connections {
            target: decoration
            onShadeChanged: {
                if (button.buttonType != "L") {
                    return;
                }
                button.toggled = decoration.shade;
            }
            onKeepBelowChanged: {
                if (button.buttonType != "B") {
                    return;
                }
                button.toggled = decoration.keepBelow;
            }
            onKeepAboveChanged: {
                if (button.buttonType != "F") {
                    return;
                }
                button.toggled = decoration.keepAbove;
            }
            onDesktopChanged: {
                if (button.buttonType != "S") {
                    return;
                }
                button.toggled = decoration.onAllDesktops;
            }
        }
    }
}
