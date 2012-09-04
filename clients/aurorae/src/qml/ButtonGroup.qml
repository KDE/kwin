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
    function createButtons() {
        for (var i=0; i<buttons.length; i++) {
            var component = undefined;
            switch (buttons.charAt(i)) {
            case "_":
                var button = Qt.createQmlObject("import QtQuick 1.1; Item{ width: " + group.explicitSpacer + "; height: parent.height}", groupRow, "dynamicGroup_" + buttons + i);
                break;
            case "A":
                component = group.maximizeButton;
                break;
            case "B":
                component = group.keepBelowButton;
                break;
            case "F":
                component = group.keepAboveButton;
                break;
            case "H":
                component = group.helpButton;
                break;
            case "I":
                component = group.minimizeButton;
                break;
            case "L":
                component = group.shadeButton;
                break;
            case "M":
                component = group.menuButton;
                break;
            case "S":
                component = group.allDesktopsButton;
                break;
            case "X":
                component = group.closeButton;
                break;
            }
            if (!component) {
                continue;
            }
            var button = Qt.createQmlObject("import QtQuick 1.1; Loader{}", groupRow, "dynamicGroup_" + buttons + i);
            button.sourceComponent = component;
        }
    }
    id: group
    property string buttons
    property int explicitSpacer
    property alias spacing: groupRow.spacing

    property variant closeButton
    property variant helpButton
    property variant keepAboveButton
    property variant keepBelowButton
    property variant maximizeButton
    property variant menuButton
    property variant minimizeButton
    property variant allDesktopsButton
    property variant shadeButton

    width: childrenRect.width

    Row {
        id: groupRow
    }
    onButtonsChanged: {
        for (var i = 0; i < groupRow.children.length; i++) {
            groupRow.children[i].destroy();
        }
        createButtons();
    }
}
