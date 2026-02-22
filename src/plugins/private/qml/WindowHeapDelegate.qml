/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
    SPDX-FileCopyrightText: 2024 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.plasma.components as PC3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.ksvg as KSvg

ExpoCell {
    id: thumb

    required property QtObject window
    required property int index
    required property Item windowHeap

    readonly property bool selected: windowHeap.selectedIndex === index
    property bool gestureInProgress: effect.gestureInProgress
    // Where the internal contentItem will be parented to
    property Item contentItemParent: this

    // no desktops is a special value which means "All Desktops"
    readonly property bool presentOnCurrentDesktop: !window.desktops.length || window.desktops.indexOf(KWinComponents.Workspace.currentDesktop) !== -1
    readonly property bool initialHidden: window.minimized
    readonly property bool activeHidden: {
        if (window.skipSwitcher) {
            return true;
        } else if (windowHeap.showOnly === "activeClass") {
            if (!KWinComponents.Workspace.activeWindow) {
                return true;
            } else {
                return KWinComponents.Workspace.activeWindow.resourceName !== window.resourceName;
            }
        } else {
            return windowHeap.showOnly.length !== 0
                && windowHeap.showOnly.indexOf(window.internalId) === -1;
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

    property bool isReady: width !== 0 && height !== 0

    function restoreDND(oldGlobalRect: rect) {
        thumbSource.restoreDND(oldGlobalRect);
    }

    layout: windowHeap.layout
    shouldLayout: !thumb.activeHidden
    partialActivationFactor: effect.partialActivationFactor
    naturalX: thumb.window.x - thumb.window.output.geometry.x
    naturalY: thumb.window.y - thumb.window.output.geometry.y
    naturalWidth: thumb.window.width
    naturalHeight: thumb.window.height
    persistentKey: thumb.window.internalId
    bottomMargin: icon.height / 4 + (caption.visible ? caption.height + Kirigami.Units.smallSpacing : 0) + Kirigami.Units.largeSpacing

    Behavior on x {
        enabled: thumb.isReady
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            easing.type: Easing.InOutCubic
        }
    }
    Behavior on y {
        enabled: thumb.isReady
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            easing.type: Easing.InOutCubic
        }
    }
    Behavior on width {
        enabled: thumb.isReady
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            easing.type: Easing.InOutCubic
        }
    }
    Behavior on height {
        enabled: thumb.isReady
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            easing.type: Easing.InOutCubic
        }
    }

    contentItem: Item {
        id: mainContent
        parent: contentItemParent
        visible: opacity > 0 && (!activeHidden || !initialHidden)
        opacity: (1 - downGestureProgress) * (initialHidden ? partialActivationFactor : 1)
        z: (activeDragHandler.active || returnAnimation.running) ? 1000
            : thumb.window.stackingOrder * (presentOnCurrentDesktop ? 1 : 0.001)

        KWinComponents.WindowThumbnail {
            id: thumbSource
            wId: thumb.window.internalId
            scale: targetScale
            width: mainContent.width
            height: mainContent.height

            Binding on width {
                value: mainContent.width
                when: !returnAnimation.active
            }
            Binding on height {
                value: mainContent.height
                when: !returnAnimation.active
            }

            Drag.proposedAction: Qt.MoveAction
            Drag.supportedActions: Qt.MoveAction
            Drag.source: thumb.window
            Drag.hotSpot: Qt.point(
                thumb.activeDragHandler.centroid.pressPosition.x,
                thumb.activeDragHandler.centroid.pressPosition.y)
            Drag.keys: ["kwin-window"]

            onXChanged: effect.checkItemDraggedOutOfScreen(thumbSource)
            onYChanged: effect.checkItemDraggedOutOfScreen(thumbSource)

            function saveDND() {
                const oldGlobalRect = mapToItem(null, 0, 0, width, height);
                thumb.windowHeap.saveDND(thumb.window.internalId, oldGlobalRect);
            }
            function restoreDND(oldGlobalRect: rect) {
                const newGlobalRect = mapFromItem(null, oldGlobalRect);
                // We need proper mapping for the heap geometry because they are positioned with
                // translation transformations
                const heapRect = thumb.windowHeap.mapToItem(null, Qt.size(thumb.windowHeap.width, thumb.windowHeap.height));
                // Disable bindings
                returnAnimation.active = true;
                x = newGlobalRect.x;
                y = newGlobalRect.y;
                width = newGlobalRect.width;
                height = newGlobalRect.height;

                returnAnimation.restart();

                // If we dropped on another desktop, don't make the window fly off  the screen
                if ((oldGlobalRect.x < heapRect.x && heapRect.x + heapRect.width < oldGlobalRect.x + oldGlobalRect.width) ||
                    (oldGlobalRect.y < heapRect.y && heapRect.y + heapRect.height < oldGlobalRect.y + oldGlobalRect.height)) {
                    returnAnimation.complete();
                }
            }
            function deleteDND() {
                thumb.windowHeap.deleteDND(thumb.window.internalId);
            }

            // Not using FrameSvg hover element intentionally for stylistic reasons
            Rectangle {
                border.width: 6
                border.color: Kirigami.Theme.highlightColor
                anchors.fill: parent
                anchors.margins: -border.width
                radius: Kirigami.Units.cornerRadius
                color: "transparent"
                visible: !thumb.windowHeap.dragActive && (hoverHandler.hovered || (thumb.selected && Window.window.activeFocusItem)) && windowHeap.effectiveOrganized
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: thumb.activeDragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
            }
            ParallelAnimation {
                id: returnAnimation
                property bool active: false
                onRunningChanged: active = running
                NumberAnimation {
                    target: thumbSource
                    properties: "x,y"
                    to: 0
                    duration: thumb.windowHeap.animationDuration
                    easing.type: Easing.InOutCubic
                }
                NumberAnimation {
                    target: thumbSource
                    property: "width"
                    to: mainContent.width
                    duration: thumb.windowHeap.animationDuration
                    easing.type: Easing.InOutCubic
                }
                NumberAnimation {
                    target: thumbSource
                    property: "height"
                    to: mainContent.height
                    duration: thumb.windowHeap.animationDuration
                    easing.type: Easing.InOutCubic
                }
                NumberAnimation {
                    target: thumbSource
                    property: "scale"
                    to: 1
                    duration: thumb.windowHeap.animationDuration
                    easing.type: Easing.InOutCubic
                }
            }
        }

        PC3.Label {
            anchors.fill: thumbSource
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: i18nd("kwin", "Drag Down To Close")
            visible: !thumb.activeHidden && touchDragHandler.active
            background: Rectangle {
                anchors.centerIn: parent
                height: parent.contentHeight + Kirigami.Units.smallSpacing
                width: parent.contentWidth + Kirigami.Units.smallSpacing
                color: Kirigami.Theme.backgroundColor
                radius: Kirigami.Units.cornerRadius
            }
        }

        Kirigami.Icon {
            id: icon
            width: Kirigami.Units.iconSizes.large
            height: Kirigami.Units.iconSizes.large
            opacity: partialActivationFactor
            scale: Math.min(1.0, mainContent.width / Math.max(0.01, thumb.width))
            source: thumb.window.icon
            anchors.horizontalCenter: thumbSource.horizontalCenter
            anchors.verticalCenter: thumbSource.bottom
            anchors.verticalCenterOffset: -Math.round(height / 4) * scale
            visible: !thumb.activeHidden && !activeDragHandler.active && !returnAnimation.running
            PC3.Label {
                id: caption
                visible: thumb.window.caption.length > 0 && thumb.windowTitleVisible
                width: thumb.width
                maximumLineCount: 1
                anchors.top: parent.bottom
                anchors.topMargin: Kirigami.Units.smallSpacing
                anchors.horizontalCenter: parent.horizontalCenter
                elide: Text.ElideRight
                text: thumb.window.caption
                color: Kirigami.Theme.textColor
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                background: Rectangle {
                    anchors.centerIn: parent
                    height: parent.contentHeight + Kirigami.Units.smallSpacing
                    width: parent.contentWidth + Kirigami.Units.smallSpacing
                    color: Kirigami.Theme.backgroundColor
                    radius: Kirigami.Units.cornerRadius
                }
            }
        }

        HoverHandler {
            id: hoverHandler
            onHoveredChanged: if (hovered !== selected) {
                thumb.windowHeap.resetSelected();
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                KWinComponents.Workspace.activeWindow = thumb.window;
                thumb.windowHeap.activated();
            }
            onPressedChanged: {
                if (pressed) {
                    thumbSource.Drag.active = true;
                } else if (!thumb.activeDragHandler.active) {
                    thumbSource.Drag.active = false;
                }
            }
        }

        component DragManager : DragHandler {
            target: thumbSource
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            // This does not work when moving pointer fast and pressing along the way
            // See also QTBUG-105903, QTBUG-105904
            // enabled: thumb.state !== "active-normal"

            onActiveChanged: {
                thumb.windowHeap.dragActive = active;
                if (active) {
                    thumb.activeDragHandler = this;
                } else {
                    thumbSource.saveDND();
                    returnAnimation.restart();

                    var action = thumbSource.Drag.drop();
                    if (action === Qt.MoveAction) {
                        // This whole component is in the process of being destroyed due to drop onto
                        // another virtual desktop (not another screen).
                        if (typeof thumbSource !== "undefined") {
                            // Except the case when it was dropped on the same desktop which it's already on, so let's return to normal state anyway.
                            thumbSource.deleteDND();
                        }
                        return;
                    }

                    var globalPos = targetScreen.mapToGlobal(centroid.scenePosition);
                    effect.checkItemDroppedOutOfScreen(globalPos, thumbSource);

                    if (typeof thumbSource !== "undefined") {
                        // else, return to normal without reparenting
                        thumbSource.deleteDND();
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

        Loader {
            id: closeButton
            LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft

            anchors {
                right: thumbSource.right
                top: thumbSource.top
                margins: Kirigami.Units.smallSpacing
            }
            active: thumb.closeButtonVisible && (hoverHandler.hovered || Kirigami.Settings.tabletMode || Kirigami.Settings.hasTransientTouchInput) && thumb.window.closeable && !thumb.activeDragHandler.active && !returnAnimation.running

            sourceComponent: PC3.Button {
                text: i18ndc("kwin", "@info:tooltip as in: 'close this window'", "Close window")
                icon.name: "window-close"
                display: PC3.AbstractButton.IconOnly

                PC3.ToolTip.text: text
                PC3.ToolTip.visible: hovered && display === PC3.AbstractButton.IconOnly
                PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
                Accessible.name: text

                onClicked: thumb.window.closeWindow();
            }
        }

        Component.onDestruction: {
            if (selected) {
                windowHeap.resetSelected();
            }
        }
    }
}
