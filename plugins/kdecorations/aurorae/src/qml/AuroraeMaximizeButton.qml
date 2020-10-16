/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import org.kde.kwin.decoration 0.1

Item {
    id: button
    width: auroraeTheme.buttonWidthMaximizeRestore * auroraeTheme.buttonSizeFactor
    height: auroraeTheme.buttonHeight * auroraeTheme.buttonSizeFactor
    property bool hovered: false
    property bool pressed: false
    property bool toggled: false
    AuroraeButton {
        id: maximizeButton
        anchors.fill: parent
        buttonType: DecorationOptions.DecorationButtonMaximizeRestore
        opacity: (!decoration.client.maximized || auroraeTheme.restoreButtonPath == "") ? 1 : 0
        hovered: button.hovered
        pressed: button.pressed
        toggled: button.toggled
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    AuroraeButton {
        id: restoreButton
        anchors.fill: parent
        buttonType: DecorationOptions.DecorationButtonMaximizeRestore + 100
        opacity: (decoration.client.maximized && auroraeTheme.restoreButtonPath != "") ? 1 : 0
        hovered: button.hovered
        pressed: button.pressed
        toggled: button.toggled
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: {
            return Qt.LeftButton | Qt.RightButton | Qt.MiddleButton;
        }
        hoverEnabled: true
        onEntered: button.hovered = true
        onExited: button.hovered = false
        onPressed: button.pressed = true
        onReleased: button.pressed = false
        onClicked: {
            decoration.requestToggleMaximization(mouse.button);
        }
    }
}
