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
import QtQuick 2.0
import org.kde.kwin.decoration 0.1

Item {
    id: button
    property int buttonType : DecorationOptions.DecorationButtonNone
    property bool hovered: false
    property bool pressed: false
    property bool toggled: false
    enabled: {
        switch (button.buttonType) {
        case DecorationOptions.DecorationButtonClose:
            return decoration.closeable;
        case DecorationOptions.DecorationButtonMaximizeRestore:
            return decoration.maximizeable;
        case DecorationOptions.DecorationButtonMinimize:
            return decoration.minimizeable;
        case DecorationOptions.DecorationButtonExplicitSpacer:
            return false;
        default:
            return true;
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: {
            switch (button.buttonType) {
            case DecorationOptions.DecorationButtonMenu:
                return Qt.LeftButton | Qt.RightButton;
            case DecorationOptions.DecorationButtonMaximizeRestore:
                return Qt.LeftButton | Qt.RightButton | Qt.MiddleButton;
            default:
                return Qt.LeftButton;
            }
        }
        hoverEnabled: true
        onEntered: button.hovered = true
        onExited: button.hovered = false
        onPressed: button.pressed = true
        onReleased: button.pressed = false
        onClicked: {
            switch (button.buttonType) {
            case DecorationOptions.DecorationButtonMenu:
                // menu
                decoration.menuClicked();
                break;
            case DecorationOptions.DecorationButtonApplicationMenu:
                // app menu
                decoration.appMenuClicked();
                break;
            case DecorationOptions.DecorationButtonOnAllDesktops:
                // all desktops
                decoration.toggleOnAllDesktops();
                break;
            case DecorationOptions.DecorationButtonQuickHelp:
                // help
                decoration.showContextHelp();
                break;
            case DecorationOptions.DecorationButtonMinimize:
                // minimize
                decoration.minimize();
                break;
            case DecorationOptions.DecorationButtonMaximizeRestore:
                // maximize
                decoration.maximize(mouse.button);
                break;
            case DecorationOptions.DecorationButtonClose:
                // close
                decoration.closeWindow();
                break;
            case DecorationOptions.DecorationButtonKeepAbove:
                // keep above
                decoration.toggleKeepAbove();
                break;
            case DecorationOptions.DecorationButtonKeepBelow:
                // keep below
                decoration.toggleKeepBelow();
                break;
            case DecorationOptions.DecorationButtonShade:
                // shade
                decoration.toggleShade();
                break;
            }
        }
        onDoubleClicked: {
            if (button.buttonType == DecorationOptions.DecorationButtonMenu) {
                decoration.closeWindow();
            }
        }
        Component.onCompleted: {
            switch (button.buttonType) {
            case DecorationOptions.DecorationButtonOnAllDesktops:
                // all desktops
                button.toggled = decoration.onAllDesktops;
                break;
            case DecorationOptions.DecorationButtonKeepAbove:
                button.toggled = decoration.keepAbove;
                break;
            case DecorationOptions.DecorationButtonKeepBelow:
                button.toggled = decoration.keepBelow;
                break;
            case DecorationOptions.DecorationButtonShade:
                button.toggled = decoration.shade;
                break;
            }
        }
        Connections {
            target: decoration
            onShadeChanged: {
                if (button.buttonType != DecorationOptions.DecorationButtonShade) {
                    return;
                }
                button.toggled = decoration.shade;
            }
            onKeepBelowChanged: {
                if (button.buttonType != DecorationOptions.DecorationButtonKeepBelow) {
                    return;
                }
                button.toggled = decoration.keepBelow;
            }
            onKeepAboveChanged: {
                if (button.buttonType != DecorationOptions.DecorationButtonKeepAbove) {
                    return;
                }
                button.toggled = decoration.keepAbove;
            }
            onDesktopChanged: {
                if (button.buttonType != DecorationOptions.DecorationButtonOnAllDesktops) {
                    return;
                }
                button.toggled = decoration.onAllDesktops;
            }
        }
    }
}
