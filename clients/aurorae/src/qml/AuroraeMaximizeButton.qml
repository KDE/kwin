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
