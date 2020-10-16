/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
            return decoration.client.closeable;
        case DecorationOptions.DecorationButtonMaximizeRestore:
            return decoration.client.maximizeable;
        case DecorationOptions.DecorationButtonMinimize:
            return decoration.client.minimizeable;
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
                decoration.requestShowWindowMenu();
                break;
            case DecorationOptions.DecorationButtonApplicationMenu:
                // app menu
                var pos = button.mapToItem(null, 0, 0); // null = "map to scene"
                decoration.requestShowApplicationMenu(Qt.rect(pos.x, pos.y, button.width, button.height), 0)
                break;
            case DecorationOptions.DecorationButtonOnAllDesktops:
                // all desktops
                decoration.requestToggleOnAllDesktops();
                break;
            case DecorationOptions.DecorationButtonQuickHelp:
                // help
                decoration.requestContextHelp();
                break;
            case DecorationOptions.DecorationButtonMinimize:
                // minimize
                decoration.requestMinimize();
                break;
            case DecorationOptions.DecorationButtonMaximizeRestore:
                // maximize
                decoration.requestToggleMaximization(mouse.button);
                break;
            case DecorationOptions.DecorationButtonClose:
                // close
                decoration.requestClose();
                break;
            case DecorationOptions.DecorationButtonKeepAbove:
                // keep above
                decoration.requestToggleKeepAbove();
                break;
            case DecorationOptions.DecorationButtonKeepBelow:
                // keep below
                decoration.requestToggleKeepBelow();
                break;
            case DecorationOptions.DecorationButtonShade:
                // shade
                decoration.requestToggleShade();
                break;
            }
        }
        onDoubleClicked: {
            if (button.buttonType == DecorationOptions.DecorationButtonMenu) {
                decoration.requestClose();
            }
        }
        Component.onCompleted: {
            switch (button.buttonType) {
            case DecorationOptions.DecorationButtonOnAllDesktops:
                // all desktops
                button.toggled = Qt.binding(function() { return decoration.client.onAllDesktops; });
                break;
            case DecorationOptions.DecorationButtonKeepAbove:
                button.toggled = Qt.binding(function() { return decoration.client.keepAbove; });
                break;
            case DecorationOptions.DecorationButtonKeepBelow:
                button.toggled = Qt.binding(function() { return decoration.client.keepBelow; });
                break;
            case DecorationOptions.DecorationButtonShade:
                button.toggled = Qt.binding(function() { return decoration.client.shaded; });
                break;
            }
        }
    }
}
