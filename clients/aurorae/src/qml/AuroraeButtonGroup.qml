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

Item {
    id: group
    property string buttons

    Component.onCompleted: {
        var component = Qt.createComponent("AuroraeButton.qml");
        for (var i=0; i<buttons.length; i++) {
            if (buttons.charAt(i) == "_") {
                Qt.createQmlObject("import QtQuick 1.1; Item { width: auroraeTheme.explicitButtonSpacer; height: auroraeTheme.buttonHeight }",
                    groupRow, "explicitSpacer" + buttons + i);
            } else if (buttons.charAt(i) == "M") {
                Qt.createQmlObject("import QtQuick 1.1; MenuButton { width: auroraeTheme.buttonWidthMenu; height: auroraeTheme.buttonHeight }",
                    groupRow, "menuButton" + buttons + i);
            } else {
                component.createObject(groupRow, {buttonType: buttons.charAt(i)});
            }
        }
    }
    Row {
        id: groupRow
        spacing: auroraeTheme.buttonSpacing
    }
}
