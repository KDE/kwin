/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.1
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    PlasmaCore.Svg {
        id: glowSvg
        imagePath: "widgets/glowbar"
    }

    GridLayout {
        id: grid
        columnSpacing: 0
        rowSpacing: 0

        PlasmaCore.SvgItem {
            id: leadingItem
            Layout.preferredWidth: svg.elementSize(elementId).width
            Layout.preferredHeight: svg.elementSize(elementId).width
            svg: glowSvg
        }

        PlasmaCore.SvgItem {
            id: centerItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: svg.elementSize(elementId).width
            Layout.preferredHeight: svg.elementSize(elementId).width
            svg: glowSvg
        }

        PlasmaCore.SvgItem {
            id: trailingItem
            Layout.preferredWidth: svg.elementSize(elementId).width
            Layout.preferredHeight: svg.elementSize(elementId).width
            svg: glowSvg
        }
    }

    states: [
        State {
            name: "__horizontal"

            PropertyChanges {
                target: grid
                flow: GridLayout.LeftToRight
            }
            AnchorChanges {
                target: grid
                anchors.left: grid.parent.left
                anchors.right: grid.parent.right
            }
        },
        State {
            name: "topedge"
            extend: "__horizontal"

            AnchorChanges {
                target: grid
                anchors.top: grid.parent.top
            }
            PropertyChanges {
                target: leadingItem
                elementId: "bottomleft"
            }
            PropertyChanges {
                target: centerItem
                elementId: "bottom"
            }
            PropertyChanges {
                target: trailingItem
                elementId: "bottomright"
            }
        },
        State {
            name: "bottomedge"
            extend: "__horizontal"

            AnchorChanges {
                target: grid
                anchors.bottom: grid.parent.bottom
            }
            PropertyChanges {
                target: leadingItem
                elementId: "topleft"
            }
            PropertyChanges {
                target: centerItem
                elementId: "top"
            }
            PropertyChanges {
                target: trailingItem
                elementId: "topright"
            }
        },

        State {
            name: "__vertical"

            PropertyChanges {
                target: grid
                flow: GridLayout.TopToBottom
            }
            AnchorChanges {
                target: grid
                anchors.top: grid.parent.top
                anchors.bottom: grid.parent.bottom
            }
        },
        State {
            name: "leftedge"
            extend: "__vertical"

            AnchorChanges {
                target: grid
                anchors.left: grid.parent.left
            }
            PropertyChanges {
                target: leadingItem
                elementId: "topright"
            }
            PropertyChanges {
                target: centerItem
                elementId: "right"
            }
            PropertyChanges {
                target: trailingItem
                elementId: "bottomright"
            }
        },
        State {
            name: "rightedge"
            extend: "__vertical"

            AnchorChanges {
                target: grid
                anchors.right: grid.parent.right
            }
            PropertyChanges {
                target: leadingItem
                elementId: "topleft"
            }
            PropertyChanges {
                target: centerItem
                elementId: "left"
            }
            PropertyChanges {
                target: trailingItem
                elementId: "bottomleft"
            }
        }
    ]
}
