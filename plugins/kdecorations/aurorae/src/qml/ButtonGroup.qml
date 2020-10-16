/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
