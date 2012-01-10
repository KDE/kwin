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

DecorationButton {
    function widthForButton() {
        switch (buttonType) {
        case "M":
            // menu
            return auroraeTheme.buttonWidthMenu;
        case "S":
            // all desktops
            return auroraeTheme.buttonWidthAllDesktops;
        case "H":
            // help
            return auroraeTheme.buttonWidthHelp;
        case "I":
            // minimize
            return auroraeTheme.buttonWidthMinimize;
        case "A":
            // maximize
            return auroraeTheme.buttonWidthMaximizeRestore;
        case "X":
            // close
            return auroraeTheme.buttonWidthClose;
        case "F":
            // keep above
            return auroraeTheme.buttonWidthKeepAbove;
        case "B":
            // keep below
            return auroraeTheme.buttonWidthKeepBelow;
        case "L":
            // shade
            return auroraeTheme.buttonWidthShade;
        default:
            return auroraeTheme.buttonWidth;
        }
    }
    function pathForButton() {
        switch (buttonType) {
        case "S":
            // all desktops
            return auroraeTheme.allDesktopsButtonPath;
        case "H":
            // help
            return auroraeTheme.helpButtonPath;
        case "I":
            // minimize
            return auroraeTheme.minimizeButtonPath;
        case "A":
            // maximize
            return auroraeTheme.maximizeButtonPath;
        case "R":
            // maximize
            return auroraeTheme.restoreButtonPath;
        case "X":
            // close
            return auroraeTheme.closeButtonPath;
        case "F":
            // keep above
            return auroraeTheme.keepAboveButtonPath;
        case "B":
            // keep below
            return auroraeTheme.keepBelowButtonPath;
        case "L":
            // shade
            return auroraeTheme.shadeButtonPath;
        default:
            return "";
        }
    }
    function toggledPressedStatusChange() {
        if (pressed || toggled) {
            // pressed comes after hovered (if supported)
            if (!decoration.active && buttonSvg.supportsInactivePressed) {
                // if inactive and the buttons supports pressed, we use it
                // in case we have an inactive-hover but no pressed we stay in hovered state
                state = "inactive-pressed";
            } else if (decoration.active && state != "inactive-hover" && buttonSvg.supportsPressed) {
                state = "active-pressed";
            }
        } else {
            if (!decoration.active && hovered && buttonSvg.supportsInactiveHover) {
                state = "inactive-hover";
            } else if (!decoration.active && buttonSvg.supportsInactive) {
                state = "inactive";
            } else if (hovered && buttonSvg.supportsHover) {
                state = "active-hover";
            } else {
                state = "active";
            }
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
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "active"
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActiveHover
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "hover"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActivePressed
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "pressed"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: buttonActiveDeactivated
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "deactivated"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactive
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "inactive"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactiveHover
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "hover-inactive"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactivePressed
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "pressed-inactive"
        opacity: 0
    }
    PlasmaCore.FrameSvgItem {
        id: buttonInactiveDeactivated
        anchors.fill: parent
        imagePath: buttonSvg.imagePath
        prefix: "deactivated-inactive"
        opacity: 0
    }
    Component.onCompleted: {
        if (!decoration.active && !enabled && buttonSvg.supportsInactiveDeactivated) {
            state = "inactive-deactivated";
        } else if (!enabled && buttonSvg.supportsDeactivated) {
            state = "active-deactivated";
        } else if (!decoration.active && (toggled || pressed) && buttonSvg.supportsInactivePressed) {
            state = "inactive-pressed";
        } else if ((toggled || pressed) && buttonSvg.supportsPressed) {
            state = "active-pressed";
        } else if (!decoration.active && buttonSvg.supportsInactive) {
            state = "inactive";
        } else {
            state = "active";
        }
        if (buttonType == "H" && !decoration.providesContextHelp) {
            visible = false;
        } else {
            visible = buttonSvg.imagePath != "";
        }
    }
    onHoveredChanged: {
        if (state == "active-pressed" || state == "inactive-pressed") {
            // state change of hovered does not matter as the button is currently pressed
            return;
        }
        if (hovered) {
            if (state == "active" && buttonSvg.supportsHover) {
                state = "active-hover";
            } else if (state == "inactive" && buttonSvg.supportsInactiveHover) {
                state = "inactive-hover";
            }
        } else {
            if (!decoration.active && buttonSvg.supportsInactive) {
                state = "inactive";
            } else {
                state = "active";
            }
        }
    }
    onPressedChanged: toggledPressedStatusChange()
    onToggledChanged: toggledPressedStatusChange()
    states: [
        State { name: "active" },
        State { name: "active-hover" },
        State { name: "active-pressed" },
        State { name: "active-deactivated" },
        State { name: "inactive" },
        State { name: "inactive-hover" },
        State { name: "inactive-pressed" },
        State { name: "inactive-deactivated" }
    ]
    transitions: [
        Transition {
            to: "active"
            // Cross fade from somewhere to active
            ParallelAnimation {
                NumberAnimation { target: buttonActive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "active-hover"
            // Cross fade from active to hover
            ParallelAnimation {
                NumberAnimation { target: buttonActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActiveHover; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "active-pressed"
            // Cross fade to pressed state
            ParallelAnimation {
                NumberAnimation { target: buttonActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActivePressed; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "inactive"
            // Cross fade from hover/pressed/active to inactive
            ParallelAnimation {
                NumberAnimation { target: buttonActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactive; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "inactive-hover"
            // Cross fade from inactive to hover
            ParallelAnimation {
                NumberAnimation { target: buttonActiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveHover; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "inactive-pressed"
            // Cross fade to inactive pressed state
            ParallelAnimation {
                NumberAnimation { target: buttonActivePressed; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveHover; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactivePressed; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "active-deactivated"
            ParallelAnimation {
                NumberAnimation { target: buttonActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActiveDeactivated; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveDeactivated; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
            }
        },
        Transition {
            to: "inactive-deactivated"
            ParallelAnimation {
                NumberAnimation { target: buttonActive; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonActiveDeactivated; property: "opacity"; to: 0; duration: auroraeTheme.animationTime }
                NumberAnimation { target: buttonInactiveDeactivated; property: "opacity"; to: 1; duration: auroraeTheme.animationTime }
            }
        }
    ]
    Connections {
        target: decoration
        onActiveChanged: {
            if (!decoration.active && !enabled && buttonSvg.supportsInactiveDeactivated) {
                state = "inactive-deactivated";
            } else if (!enabled && buttonSvg.supportsDeactivated) {
                state = "active-deactivated";
            } else if (!decoration.active && (pressed || toggled) && buttonSvg.supportsInactivePressed) {
                state = "inactive-pressed";
            } else if (!decoration.active && hovered && buttonSvg.supportsInactiveHover) {
                state = "inactive-hover";
            } else if (!decoration.active && buttonSvg.supportsInactive) {
                state = "inactive";
            } else if ((pressed || toggled) && buttonSvg.supportsPressed) {
                state = "active-pressed";
            } else if (hovered && buttonSvg.supportsHover) {
                state = "active-hover";
            } else {
                state = "active";
            }
        }
    }
}
