/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Window 2.12
import org.kde.kirigami 2.12 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore


Item {
    id: thumb

    required property QtObject client
    required property int index
    required property Item windowHeap

    readonly property alias dragHandler: thumb.activeDragHandler
    readonly property bool selected: thumb.windowHeap.selectedIndex == index
    //TODO: move?
    readonly property bool hidden: {
        if (thumb.windowHeap.showOnly === "activeClass") {
            return thumb.windowHeap.activeClass !== String(thumb.client.resourceName); // thumb.client.resourceName is not an actual String as comes from a QByteArray so === would fail
        } else {
            return thumb.windowHeap.showOnly.length && thumb.windowHeap.showOnly.indexOf(client.internalId) == -1;
        }
    }

    // Show a close button on this thumbnail
    property bool closeButtonVisible: true
    // Show a text label under this thumbnail
    property bool windowTitleVisible: true

    //scale up and down the whole thumbnail without affecting layouting
    property real targetScale: 1.0

    // Swipe down gesture by touch, in some effects will close the window
    readonly property alias downGestureProgress: touchDragHandler.downGestureProgress
    signal downGestureTriggered()



    Component.onCompleted: {
        if (thumb.client.active) {
            thumb.windowHeap.activeClass = thumb.client.resourceName;
        }
    }
    Connections {
        target: thumb.client
        function onActiveChanged() {
            if (thumb.client.active) {
                thumb.windowHeap.activeClass = thumb.client.resourceName;
            }
        }
    }

    state: {
        if (effect.gestureInProgress) {
            return "partial";
        }
        if (thumb.windowHeap.effectiveOrganized) {
            return hidden ? "active-hidden" : "active";
        }
        return client.minimized ? "initial-minimized" : "initial";
    }

    visible: opacity > 0
    z: thumb.activeDragHandler.active ? 1000
        : client.stackingOrder + (thumb.client.desktop == KWinComponents.Workspace.currentDesktop ? 100 : 0)

    component TweenBehavior : Behavior {
        enabled: thumb.state !== "partial" && thumb.windowHeap.animationEnabled && !thumb.activeDragHandler.active
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    TweenBehavior on x {}
    TweenBehavior on y {}
    TweenBehavior on width {}
    TweenBehavior on height {}

    KWinComponents.WindowThumbnailItem {
        id: thumbSource
        wId: thumb.client.internalId
        state: thumb.activeDragHandler.active ? "drag" : "normal"
        readonly property QtObject screen: targetScreen
        readonly property QtObject client: thumb.client

        Drag.active: thumb.activeDragHandler.active
        Drag.source: thumb.client
        Drag.hotSpot: Qt.point(
            thumb.activeDragHandler.centroid.pressPosition.x * thumb.targetScale,
            thumb.activeDragHandler.centroid.pressPosition.y * thumb.targetScale)

        onXChanged: effect.checkItemDraggedOutOfScreen(thumbSource)
        onYChanged: effect.checkItemDraggedOutOfScreen(thumbSource)

        states: [
            State {
                name: "normal"
                PropertyChanges {
                    target: thumbSource
                    x: 0
                    y: 0
                    width: thumb.width * thumb.targetScale
                    height: thumb.height * thumb.targetScale
                }
            },
            State {
                name: "drag"
                PropertyChanges {
                    target: thumbSource
                    x: -thumb.activeDragHandler.centroid.pressPosition.x * thumb.targetScale +
                            thumb.activeDragHandler.centroid.position.x
                    y: -thumb.activeDragHandler.centroid.pressPosition.y * thumb.targetScale +
                            thumb.activeDragHandler.centroid.position.y
                    width: cell.width * thumb.targetScale
                    height: cell.height * thumb.targetScale
                }
            }
        ]
        transitions: Transition {
            to: "normal"
            enabled: thumb.windowHeap.animationEnabled
            NumberAnimation {
                duration: thumb.windowHeap.animationDuration
                properties: "x, y, width, height, opacity"
                easing.type: Easing.OutCubic
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: thumb.activeDragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
        }
    }

    PC3.Label {
        anchors.fill: thumbSource
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: i18nd("kwin_effects", "Drag Down To Close")
        opacity: 1 - thumbSource.opacity
        visible: !thumb.hidden
    }

    PlasmaCore.IconItem {
        id: icon
        width: PlasmaCore.Units.iconSizes.large
        height: width
        source: thumb.client.icon
        usesPlasmaTheme: false
        anchors.horizontalCenter: thumbSource.horizontalCenter
        anchors.bottom: thumbSource.bottom
        anchors.bottomMargin: -height / 4
        visible: !thumb.hidden && !activeDragHandler.active


        PC3.Label {
            id: caption
            visible: thumb.windowTitleVisible
            width: Math.min(implicitWidth, thumbSource.width)
            anchors.top: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            elide: Text.ElideRight
            text: thumb.client.caption
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ExpoCell {
        id: cell
        layout: windowHeap.layout
        enabled: !thumb.hidden
        naturalX: thumb.client.x - targetScreen.geometry.x - windowHeap.layout.Kirigami.ScenePosition.x
        naturalY: thumb.client.y - targetScreen.geometry.y - windowHeap.layout.Kirigami.ScenePosition.y
        naturalWidth: thumb.client.width
        naturalHeight: thumb.client.height
        persistentKey: thumb.client.internalId
        bottomMargin: icon.height / 4 + caption.height
    }

    states: [
        State {
            name: "initial"
            PropertyChanges {
                target: thumb
                x: thumb.client.x - targetScreen.geometry.x - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.x : 0)
                y: thumb.client.y - targetScreen.geometry.y - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.y : 0)
                width: thumb.client.width
                height: thumb.client.height
            }
            PropertyChanges {
                target: icon
                opacity: 0
            }
            PropertyChanges {
                target: closeButton
                opacity: 0
            }
        },
        State {
            name: "partial"
            PropertyChanges {
                target: thumb
                x: (thumb.client.x - targetScreen.geometry.x - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.x : 0)) * (1 - effect.partialActivationFactor) + cell.x * effect.partialActivationFactor
                y: (thumb.client.y - targetScreen.geometry.y - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.y : 0)) * (1 - effect.partialActivationFactor) + cell.y * effect.partialActivationFactor
                width: thumb.client.width * (1 - effect.partialActivationFactor) + cell.width * effect.partialActivationFactor
                height: thumb.client.height * (1 - effect.partialActivationFactor) + cell.height * effect.partialActivationFactor
                opacity: thumb.client.minimized || thumb.client.desktop != KWinComponents.Workspace.currentDesktop ? effect.partialActivationFactor : 1
            }
            PropertyChanges {
                target: icon
                opacity: effect.partialActivationFactor
            }
            PropertyChanges {
                target: closeButton
                opacity: effect.partialActivationFactor
            }
        },
        State {
            name: "initial-minimized"
            extend: "initial"
            PropertyChanges {
                target: thumb
                opacity: 0
            }
            PropertyChanges {
                target: icon
                opacity: 0
            }
            PropertyChanges {
                target: closeButton
                opacity: 0
            }
        },
        State {
            name: "active"
            PropertyChanges {
                target: thumb
                x: cell.x
                y: cell.y
                width: cell.width
                height: cell.height
            }
            PropertyChanges {
                target: icon
                opacity: 1
            }
            PropertyChanges {
                target: closeButton
                opacity: 1
            }
        },
        State {
            name: "active-hidden"
            extend: "active"
            PropertyChanges {
                target: thumb
                opacity: 0
            }
        }
    ]

    transitions: Transition {
        to: "initial, active, active-hidden"
        enabled: thumb.windowHeap.animationEnabled
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            properties: "x, y, width, height, opacity"
            easing.type: Easing.InOutCubic
        }
    }


    PlasmaCore.FrameSvgItem {
        anchors {
            fill: parent
            topMargin: -PlasmaCore.Units.smallSpacing * 2
            leftMargin: -PlasmaCore.Units.smallSpacing * 2
            rightMargin: -PlasmaCore.Units.smallSpacing * 2
            bottomMargin: -(Math.round(icon.height / 4) + caption.height + (PlasmaCore.Units.smallSpacing * 2))
        }
        imagePath: "widgets/viewitem"
        prefix: "hover"
        z: -1
        visible: !thumb.activeDragHandler.active && (hoverHandler.hovered || selected)
    }

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: if (hovered != selected) {
            thumb.windowHeap.resetSelected();
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            KWinComponents.Workspace.activeClient = thumb.client;
            thumb.windowHeap.activated();
        }
    }

    TapHandler {
        acceptedPointerTypes: PointerDevice.GenericPointer | PointerDevice.Pen
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        onTapped: {
            thumb.windowHeap.windowClicked(thumb.client, eventPoint)
        }
    }

    component DragManager : DragHandler {
        id: dragHandler
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything

        onActiveChanged: {
            thumb.windowHeap.dragActive = active;
            if (active) {
                thumb.activeDragHandler = dragHandler;
            } else {
                thumbSource.Drag.drop();
                var globalPos = targetScreen.mapToGlobal(centroid.scenePosition);
                effect.checkItemDroppedOutOfScreen(globalPos, thumbSource);
            }
        }
    }
    property DragManager activeDragHandler: dragHandler
    DragManager {
        id: dragHandler
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
    }
    DragManager {
        id: touchDragHandler
        acceptedDevices: PointerDevice.TouchScreen
        readonly property double downGestureProgress: {
            if (!active) {
                return 0.0;
            }

            const startDistance = thumb.windowHeap.Kirigami.ScenePosition.y + thumb.windowHeap.height - centroid.scenePressPosition.y;
            const localPosition = thumb.windowHeap.Kirigami.ScenePosition.y + thumb.windowHeap.height - centroid.scenePosition.y;
            return 1 - Math.min(localPosition/startDistance, 1);
        }

        onActiveChanged: {
            if (!active) {
                if (downGestureProgress > 0.6) {
                    thumb.downGestureTriggered();
                }
            }
        }
    }

    PC3.Button {
        id: closeButton

        anchors {
            right: thumbSource.right
            top: thumbSource.top
            margins: PlasmaCore.Units.smallSpacing
        }

        visible: thumb.closeButtonVisible && (hoverHandler.hovered || Kirigami.Settings.tabletMode || Kirigami.Settings.hasTransientTouchInput) && thumb.client.closeable && !dragHandler.active
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft

        text: i18ndc("kwin_effects", "@info:tooltip as in: 'close this window'", "Close window")
        icon.name: "window-close"
        display: PC3.AbstractButton.IconOnly

        PC3.ToolTip.text: text
        PC3.ToolTip.visible: hovered && display === PC3.AbstractButton.IconOnly
        PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
        Accessible.name: text

        onClicked: thumb.client.closeWindow();
    }

    Component.onDestruction: {
        if (selected) {
            thumb.windowHeap.resetSelected();
        }
    }
}
