/*
 *  SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents

Item {
    id: root

    // Kwin::Effect
    required property QtObject effect

    // Ideally we want a gridUnit around the background, but we must account
    // for the shadow we draw providing some of that desired margin internally
    readonly property real margins: Math.max(Kirigami.Units.gridUnit - (background.x - shadow.x), 0)

    implicitWidth: shadow.width
    implicitHeight: shadow.height

    // Manage opacity for the overlay, matching the behaviour of the zoom effect
    // TODO: Opacity isn't really correctly managed with pinch, but not sure what we can do
    layer.enabled: true
    opacity: (effect.targetZoom === 1.0) ? 0 : 1
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.Linear } }

    KSvg.FrameSvgItem {
        id: shadow
        anchors.fill: background
        anchors.topMargin: -margins.top
        anchors.leftMargin: -margins.left
        anchors.rightMargin: -margins.right
        anchors.bottomMargin: -margins.bottom

        imagePath: "widgets/tooltip"
        prefix: "shadow"
    }

    KSvg.FrameSvgItem {
        id: background
        anchors.fill: container
        anchors.topMargin: -margins.top
        anchors.leftMargin: -margins.left
        anchors.rightMargin: -margins.right
        anchors.bottomMargin: -margins.bottom

        imagePath: "widgets/tooltip"
    }

    Item {
        id: container

        anchors.centerIn: parent

        width: content.implicitWidth + (content.anchors.margins * 2)
        height: content.implicitHeight + (content.anchors.margins * 2)

        ColumnLayout {
            id: content
            anchors.centerIn: parent
            anchors.margins: Kirigami.Units.largeSpacing

            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                id: zoomIcon
                implicitHeight: Kirigami.Units.iconSizes.medium
                implicitWidth: Kirigami.Units.iconSizes.medium

                property real previousTargetZoom: 1.0
                property bool isZoomingIn: true

                Connections {
                    target: root.effect
                    function onTargetZoomChanged() {
                        zoomIcon.isZoomingIn = root.effect.targetZoom > zoomIcon.previousTargetZoom;
                        zoomIcon.previousTargetZoom = root.effect.targetZoom;
                    }
                }

                animated: true
                source: {
                    if (root.effect.targetZoom >= root.effect.pixelGridZoom) {
                        return "zoom-pixels"; // TODO: Symbolic icon is different
                    } else if (zoomIcon.isZoomingIn) {
                        return "zoom-in-symbolic";
                    } else {
                        return "zoom-out-symbolic"
                    }
                }
                fallback: {
                    if (zoomIcon.isZoomingIn) {
                        return "zoom-in-symbolic";
                    } else {
                        return "zoom-out-symbolic"
                    }
                }
            }

            PlasmaComponents.Label {
                text: xi18nc("@info", "Hold <shortcut>%1</shortcut> and scroll to zoom in and out",
                             root.effect.shortcutsHintModel.pointerAxisGestureShortcut);
                textFormat: Text.RichText

                visible: root.effect.shortcutsHintModel.pointerAxisGestureShortcut.length > 0
            }

            PlasmaComponents.Label {
                text: i18nc("@info", "Pinch with three fingers to zoom in and out");

                visible: root.effect.shortcutsHintModel.hasTouchpad
            }

            Kirigami.FormLayout {

                Repeater {
                    model: root.effect.shortcutsHintModel
                    delegate: PlasmaComponents.Label {
                        Kirigami.FormData.label: model.text

                        text: model.shortcuts
                        textFormat: Text.RichText

                        visible: text.length > 0
                    }
                }
            }
        }
    }
}
