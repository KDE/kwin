/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Window 2.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: thumb

    required property QtObject client
    required property int index
    required property Item windowHeap

    readonly property bool selected: windowHeap.selectedIndex === index

    // -1 is a special value which means "All Desktops"
    readonly property bool presentOnCurrentDesktop: client.desktop === KWinComponents.Workspace.currentDesktop || client.desktop === -1
    readonly property bool initialHidden: client.minimized || !presentOnCurrentDesktop
    readonly property bool activeHidden: {
        if (windowHeap.showOnly === "activeClass") {
            if (!KWinComponents.Workspace.activeClient) {
                return true;
            } else {
                return KWinComponents.Workspace.activeClient.resourceName !== client.resourceName;
            }
        } else {
            return windowHeap.showOnly.length !== 0
                && windowHeap.showOnly.indexOf(client.internalId) === -1;
        }
    }

    // Show a close button on this thumbnail
    property bool closeButtonVisible: true
    // Show a text label under this thumbnail
    property bool windowTitleVisible: true

    // Same as for window heap
    property bool animationEnabled: false

    //scale up and down the whole thumbnail without affecting layouting
    property real targetScale: 1.0

    property DragManager activeDragHandler: dragHandler

    // Swipe down gesture by touch, in some effects will close the window
    readonly property alias downGestureProgress: touchDragHandler.downGestureProgress
    signal downGestureTriggered()

    // "normal" | "pressed" | "drag" | "reparenting"
    property string substate: "normal"

    state: {
        if (effect.gestureInProgress) {
            return "partial";
        }
        if (windowHeap.effectiveOrganized) {
            return activeHidden ? "active-hidden" : `active-${substate}`;
        }
        return initialHidden ? "initial-hidden" : "initial";
    }

    visible: opacity > 0
    z: (activeDragHandler.active || returning.running) ? 1000
        : client.stackingOrder * (presentOnCurrentDesktop ? 1 : 0.001)

    function restoreDND(oldGlobalRect: rect) {
        thumbSource.restoreDND(oldGlobalRect);
    }

    component TweenBehavior : Behavior {
        enabled: thumb.state !== "partial" && thumb.windowHeap.animationEnabled && thumb.animationEnabled && !thumb.activeDragHandler.active
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

        Drag.proposedAction: Qt.MoveAction
        Drag.supportedActions: Qt.MoveAction
        Drag.source: thumb.client
        Drag.hotSpot: Qt.point(
            thumb.activeDragHandler.centroid.pressPosition.x * thumb.targetScale,
            thumb.activeDragHandler.centroid.pressPosition.y * thumb.targetScale)

        onXChanged: effect.checkItemDraggedOutOfScreen(thumbSource)
        onYChanged: effect.checkItemDraggedOutOfScreen(thumbSource)

        function saveDND() {
            const oldGlobalRect = mapToItem(null, 0, 0, width, height);
            thumb.windowHeap.saveDND(thumb.client.internalId, oldGlobalRect);
        }
        function restoreDND(oldGlobalRect: rect) {
            thumb.substate = "reparenting";

            const newGlobalRect = mapFromItem(null, oldGlobalRect);

            x = newGlobalRect.x;
            y = newGlobalRect.y;
            width = newGlobalRect.width;
            height = newGlobalRect.height;

            thumb.substate = "normal";
        }
        function deleteDND() {
            thumb.windowHeap.deleteDND(thumb.client.internalId);
        }

        PlasmaCore.FrameSvgItem {
            anchors {
                fill: parent
                topMargin: -PlasmaCore.Units.smallSpacing * 2
                leftMargin: -PlasmaCore.Units.smallSpacing * 2
                rightMargin: -PlasmaCore.Units.smallSpacing * 2
                bottomMargin: -(Math.round(icon.height / 4) + (thumb.windowTitleVisible ? caption.height : 0) + (PlasmaCore.Units.smallSpacing * 2))
            }
            imagePath: "widgets/viewitem"
            prefix: "hover"
            z: -1
            visible: !thumb.windowHeap.dragActive && (hoverHandler.hovered || (thumb.selected && Window.window.activeFocusItem)) && windowHeap.effectiveOrganized
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
        visible: !thumb.activeHidden
    }

    PlasmaCore.IconItem {
        id: icon
        width: PlasmaCore.Units.iconSizes.large
        height: PlasmaCore.Units.iconSizes.large
        source: thumb.client.icon
        usesPlasmaTheme: false
        anchors.horizontalCenter: thumbSource.horizontalCenter
        anchors.bottom: thumbSource.bottom
        anchors.bottomMargin: -Math.round(height / 4)
        visible: !thumb.activeHidden && !activeDragHandler.active

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
        enabled: !thumb.activeHidden
        naturalX: thumb.client.x
        naturalY: thumb.client.y
        naturalWidth: thumb.client.width
        naturalHeight: thumb.client.height
        persistentKey: thumb.client.internalId
        bottomMargin: icon.height / 4 + (thumb.windowTitleVisible ? caption.height : 0)
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
                target: thumbSource
                x: 0
                y: 0
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
                opacity: thumb.initialHidden
                    ? (thumb.activeHidden ? 0 : effect.partialActivationFactor)
                    : (thumb.activeHidden ? 1 - effect.partialActivationFactor : 1)
            }
            PropertyChanges {
                target: thumbSource
                x: 0
                y: 0
                width: thumb.width
                height: thumb.height
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
            name: "initial-hidden"
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
            name: "active-hidden"
            extend: "initial-hidden"
        },
        State {
            // this state is never directly used without a substate
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
            name: "active-normal"
            extend: "active"
            PropertyChanges {
                target: thumbSource
                x: 0
                y: 0
                width: cell.width
                height: cell.height
            }
        },
        State {
            name: "active-pressed"
            extend: "active"
            PropertyChanges {
                target: thumbSource
                width: cell.width
                height: cell.height
            }
        },
        State {
            name: "active-drag"
            extend: "active"
            PropertyChanges {
                target: thumbSource
                x: -thumb.activeDragHandler.centroid.pressPosition.x * thumb.targetScale +
                        thumb.activeDragHandler.centroid.position.x
                y: -thumb.activeDragHandler.centroid.pressPosition.y * thumb.targetScale +
                        thumb.activeDragHandler.centroid.position.y
                width: cell.width * thumb.targetScale
                height: cell.height * thumb.targetScale
            }
        },
        State {
            name: "active-reparenting"
            extend: "active"
        }
    ]

    transitions: [
        Transition {
            id: returning
            from: "active-drag, active-reparenting"
            to: "active-normal"
            enabled: thumb.windowHeap.animationEnabled
            NumberAnimation {
                duration: thumb.windowHeap.animationDuration
                properties: "x, y, width, height"
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            to: "initial, initial-hidden, active-normal, active-hidden"
            enabled: thumb.windowHeap.animationEnabled
            NumberAnimation {
                duration: thumb.windowHeap.animationDuration
                properties: "x, y, width, height, opacity"
                easing.type: Easing.OutCubic
            }
        }
    ]

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: if (hovered !== selected) {
            thumb.windowHeap.resetSelected();
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            KWinComponents.Workspace.activeClient = thumb.client;
            thumb.windowHeap.activated();
        }
        onPressedChanged: {
            if (pressed) {
                var saved = Qt.point(thumbSource.x, thumbSource.y);
                thumbSource.Drag.active = true;
                thumb.substate = "pressed";
                thumbSource.x = saved.x;
                thumbSource.y = saved.y;
            } else if (!thumb.activeDragHandler.active) {
                thumbSource.Drag.active = false;
                thumb.substate = "normal";
            }
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
        target: null
        dragThreshold: 0
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        // This does not work when moving pointer fast and pressing along the way
        // See also QTBUG-105903, QTBUG-105904
        // enabled: thumb.state !== "active-normal"

        onActiveChanged: {
            thumb.windowHeap.dragActive = active;
            if (active) {
                thumb.activeDragHandler = this;
                thumb.substate = "drag";
            } else {
                thumbSource.saveDND();

                var action = thumbSource.Drag.drop();
                if (action === Qt.MoveAction) {
                    // This whole component is in the process of being destroyed due to drop onto
                    // another virtual desktop (not another screen).
                    if (typeof thumbSource !== "undefined") {
                        // Except the case when it was dropped on the same desktop which it's already on, so let's return to normal state anyway.
                        thumbSource.deleteDND();
                        thumb.substate = "normal";
                    }
                    return;
                }

                var globalPos = targetScreen.mapToGlobal(centroid.scenePosition);
                effect.checkItemDroppedOutOfScreen(globalPos, thumbSource);

                if (typeof thumbSource !== "undefined") {
                    // else, return to normal without reparenting
                    thumbSource.deleteDND();
                    thumb.substate = "normal";
                }
            }
        }
    }

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

        visible: thumb.closeButtonVisible && (hoverHandler.hovered || Kirigami.Settings.tabletMode || Kirigami.Settings.hasTransientTouchInput) && thumb.client.closeable && !thumb.activeDragHandler.active
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
            windowHeap.resetSelected();
        }
    }
}
