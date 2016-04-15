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
    function createButtons() {
        for (var i=0; i<buttons.length; i++) {
            var component = undefined;
            switch (buttons[i]) {
            case DecorationOptions.DecorationButtonExplicitSpacer:
                component = explicitSpacerComponent;
                break;
            case DecorationOptions.DecorationButtonMaximizeRestore:
                component = group.maximizeButton;
                break;
            case DecorationOptions.DecorationButtonKeepBelow:
                component = group.keepBelowButton;
                break;
            case DecorationOptions.DecorationButtonKeepAbove:
                component = group.keepAboveButton;
                break;
            case DecorationOptions.DecorationButtonQuickHelp:
                component = group.helpButton;
                break;
            case DecorationOptions.DecorationButtonMinimize:
                component = group.minimizeButton;
                break;
            case DecorationOptions.DecorationButtonShade:
                component = group.shadeButton;
                break;
            case DecorationOptions.DecorationButtonMenu:
                component = group.menuButton;
                break;
            case DecorationOptions.DecorationButtonApplicationMenu:
                component = group.appMenuButton;
                break;
            case DecorationOptions.DecorationButtonOnAllDesktops:
                component = group.allDesktopsButton;
                break;
            case DecorationOptions.DecorationButtonClose:
                component = group.closeButton;
                break;
            }
            if (!component) {
                continue;
            }
            var button = Qt.createQmlObject("import QtQuick 2.0; Loader{}", groupRow, "dynamicGroup_" + buttons + i);
            button.sourceComponent = component;
        }
    }
    id: group
    property variant buttons
    property int explicitSpacer
    property alias spacing: groupRow.spacing

    property variant closeButton
    property variant helpButton
    property variant keepAboveButton
    property variant keepBelowButton
    property variant maximizeButton
    property variant menuButton
    property variant appMenuButton
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

    Component {
        id: explicitSpacerComponent
        Item {
            width: group.explicitSpacer
            height: groupRow.height
        }
    }
}
