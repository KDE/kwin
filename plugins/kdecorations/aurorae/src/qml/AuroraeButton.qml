/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kwin.decoration 0.1

DecorationButton {
    function widthForButton() {
        switch (buttonType) {
        case DecorationOptions.DecorationButtonMenu:
            // menu
            return auroraeTheme.buttonWidthMenu;
        case DecorationOptions.DecorationButtonApplicationMenu:
            // app menu
            return auroraeTheme.buttonWidthAppMenu;
        case DecorationOptions.DecorationButtonOnAllDesktops:
            // all desktops
            return auroraeTheme.buttonWidthAllDesktops;
        case DecorationOptions.DecorationButtonQuickHelp:
            // help
            return auroraeTheme.buttonWidthHelp;
        case DecorationOptions.DecorationButtonMinimize:
            // minimize
            return auroraeTheme.buttonWidthMinimize;
        case DecorationOptions.DecorationButtonMaximizeRestore:
            // maximize
            return auroraeTheme.buttonWidthMaximizeRestore;
        case DecorationOptions.DecorationButtonClose:
            // close
            return auroraeTheme.buttonWidthClose;
        case DecorationOptions.DecorationButtonKeepAbove:
            // keep above
            return auroraeTheme.buttonWidthKeepAbove;
        case DecorationOptions.DecorationButtonKeepBelow:
            // keep below
            return auroraeTheme.buttonWidthKeepBelow;
        case DecorationOptions.DecorationButtonShade:
            // shade
            return auroraeTheme.buttonWidthShade;
        default:
            return auroraeTheme.buttonWidth;
        }
    }
    function pathForButton() {
        switch (buttonType) {
        case DecorationOptions.DecorationButtonOnAllDesktops:
            // all desktops
            return auroraeTheme.allDesktopsButtonPath;
        case DecorationOptions.DecorationButtonQuickHelp:
            // help
            return auroraeTheme.helpButtonPath;
        case DecorationOptions.DecorationButtonMinimize:
            // minimize
            return auroraeTheme.minimizeButtonPath;
        case DecorationOptions.DecorationButtonMaximizeRestore:
            // maximize
            return auroraeTheme.maximizeButtonPath;
        case DecorationOptions.DecorationButtonMaximizeRestore + 100:
            // maximize
            return auroraeTheme.restoreButtonPath;
        case DecorationOptions.DecorationButtonClose:
            // close
            return auroraeTheme.closeButtonPath;
        case DecorationOptions.DecorationButtonKeepAbove:
            // keep above
            return auroraeTheme.keepAboveButtonPath;
        case DecorationOptions.DecorationButtonKeepBelow:
            // keep below
            return auroraeTheme.keepBelowButtonPath;
        case DecorationOptions.DecorationButtonShade:
            // shade
            return auroraeTheme.shadeButtonPath;
        default:
            return "";
        }
    }
    width: widthForButton() * auroraeTheme.buttonSizeFactor
    height: auroraeTheme.buttonHeight * auroraeTheme.buttonSizeFactor
    PlasmaCore.FrameSvg {
        property bool supportsHover: hasElementPrefix("hover")
        property bool supportsPressed: hasElementPrefix("pressed")
        property bool supportsDeactivated: hasElementPrefix("deactivated")
        property bool supportsInactive: hasElementPrefix("inactive")
        property bool supportsInactiveHover: hasElementPrefix("hover-inactive")
        property bool supportsInactivePressed: hasElementPrefix("pressed-inactive")
        property bool supportsInactiveDeactivated: hasElementPrefix("deactivated-inactive")
        id: buttonSvg
        imagePath: pathForButton()
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActive
        property bool shown: (decoration.client.active || !buttonSvg.supportsInactive) && ((!pressed && !toggled) || !buttonSvg.supportsPressed) && (!hovered || !buttonSvg.supportsHover) && (enabled || !buttonSvg.supportsDeactivated)
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "active"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActiveHover
        property bool shown: hovered && !pressed && !toggled && buttonSvg.supportsHover && (decoration.client.active || !buttonSvg.supportsInactiveHover)
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "hover"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActivePressed
        property bool shown: (toggled || pressed) && buttonSvg.supportsPressed && (decoration.client.active || !buttonSvg.supportsInactivePressed)
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "pressed"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActiveDeactivated
        property bool shown: !enabled && buttonSvg.supportsDeactivated && (decoration.client.active || !buttonSvg.supportsInactiveDeactivated)
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "deactivated"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactive
        property bool shown: !decoration.client.active && buttonSvg.supportsInactive && !hovered && !pressed && !toggled && enabled
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "inactive"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactiveHover
        property bool shown: !decoration.client.active && hovered && !pressed && !toggled && buttonSvg.supportsInactiveHover
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "hover-inactive"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactivePressed
        property bool shown: !decoration.client.active && (toggled || pressed) && buttonSvg.supportsInactivePressed
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "pressed-inactive"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactiveDeactivated
        property bool shown: !decoration.client.active && !enabled && buttonSvg.supportsInactiveDeactivated
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "deactivated-inactive"
        opacity: shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: auroraeTheme.animationTime
            }
        }
    }
    Component.onCompleted: {
        if (buttonType == DecorationOptions.DecorationButtonQuickHelp && !decoration.client.providesContextHelp) {
            visible = false;
        } else {
            visible = buttonSvg.imagePath != "";
        }
    }
}
