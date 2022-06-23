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

FocusScope {
    id: heap

    enum Direction {
        Left,
        Right,
        Up,
        Down
    }

    property bool supportsCloseWindows: false
    property bool supportsDragUpGesture: false
    property bool showCaptions: true
    property alias model: windowsRepeater.model
    property alias layout: expoLayout.mode
    property int selectedIndex: -1
    property int animationDuration: PlasmaCore.Units.longDuration
    property bool animationEnabled: false
    property bool absolutePositioning: true
    property real padding: 0
    property var showOnly: []
    property string activeClass
    readonly property alias count: windowsRepeater.count

    required property bool organized
    readonly property bool effectiveOrganized: expoLayout.ready && organized
    property bool dragActive: false

    signal activated()
    //TODO: for 5.26 the delegate will be a separate component instead
    signal windowClicked(QtObject window, EventPoint eventPoint)

    function activateIndex(index) {
        KWinComponents.Workspace.activeClient = windowsRepeater.itemAt(index).client;
        activated();
    }

    KWinComponents.WindowThumbnailItem {
        id: otherScreenThumbnail
        z: 2
        property KWinComponents.WindowThumbnailItem cloneOf
        visible: false
        wId: cloneOf ? cloneOf.wId : null
        width: cloneOf ? cloneOf.width : 0
        height: cloneOf ? cloneOf.height : 0
        onCloneOfChanged: {
            if (!cloneOf) {
                visible = false;
            }
        }
    }

    Connections {
        target: effect
        function onItemDraggedOutOfScreen(item, screens) {
            let found = false;

            // don't put a proxy for item's own screen
            if (screens.length === 0 || item.screen === targetScreen) {
                otherScreenThumbnail.visible = false;
                return;
            }

            for (let i in screens) {
                if (targetScreen === screens[i]) {
                    found = true;
                    let globalPos = item.screen.mapToGlobal(item.mapToItem(null, 0,0));
                    let heapRelativePos = targetScreen.mapFromGlobal(globalPos);
                    heapRelativePos = heap.mapFromItem(null, heapRelativePos.x, heapRelativePos.y);
                    otherScreenThumbnail.cloneOf = item
                    otherScreenThumbnail.x = heapRelativePos.x;
                    otherScreenThumbnail.y = heapRelativePos.y;
                    otherScreenThumbnail.visible = true;
                }
            }

            if (!found) {
                otherScreenThumbnail.visible = false;
            }
        }
        function onItemDroppedOutOfScreen(pos, item, screen) {
            if (screen === targetScreen) {
                // To actually move we neeed a screen number rather than an EffectScreen
                KWinComponents.Workspace.sendClientToScreen(item.client, KWinComponents.Workspace.screenAt(pos));
            }
        }
    }

    ExpoLayout {
        id: expoLayout
        x: heap.padding
        y: heap.padding
        width: parent.width - 2 * heap.padding
        height: parent.height - 2 * heap.padding
        spacing: PlasmaCore.Units.largeSpacing

        Repeater {
            id: windowsRepeater

            Item {
                id: thumb

                required property QtObject client
                required property int index

                readonly property bool selected: heap.selectedIndex == index
                readonly property bool hidden: {
                    if (heap.showOnly === "activeClass") {
                        return heap.activeClass !== String(thumb.client.resourceName); // thumb.client.resourceName is not an actual String as comes from a QByteArray so === would fail
                    } else {
                        return heap.showOnly.length && heap.showOnly.indexOf(client.internalId) == -1;
                    }
                }

                Component.onCompleted: {
                    if (thumb.client.active) {
                        heap.activeClass = thumb.client.resourceName;
                    }
                }
                Connections {
                    target: thumb.client
                    function onActiveChanged() {
                        if (thumb.client.active) {
                            heap.activeClass = thumb.client.resourceName;
                        }
                    }
                }

                state: {
                    if (effect.gestureInProgress) {
                        return "partial";
                    }
                    if (heap.effectiveOrganized) {
                        return hidden ? "active-hidden" : "active";
                    }
                    return client.minimized ? "initial-minimized" : "initial";
                }

                visible: opacity > 0
                z: thumb.activeDragHandler.active ? 100 : client.stackingOrder

                component TweenBehavior : Behavior {
                    enabled: thumb.state !== "partial" && heap.animationEnabled && !thumb.activeDragHandler.active
                    NumberAnimation {
                        duration: heap.animationDuration
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
                        thumb.activeDragHandler.centroid.pressPosition.x * thumb.activeDragHandler.targetScale,
                        thumb.activeDragHandler.centroid.pressPosition.y * thumb.activeDragHandler.targetScale)

                    onXChanged: effect.checkItemDraggedOutOfScreen(thumbSource)
                    onYChanged: effect.checkItemDraggedOutOfScreen(thumbSource)

                    states: [
                        State {
                            name: "normal"
                            PropertyChanges {
                                target: thumbSource
                                x: 0
                                y: 0
                                width: thumb.width
                                height: thumb.height
                            }
                        },
                        State {
                            name: "drag"
                            PropertyChanges {
                                target: thumbSource
                                x: -thumb.activeDragHandler.centroid.pressPosition.x * thumb.activeDragHandler.targetScale +
                                        thumb.activeDragHandler.centroid.position.x
                                y: -thumb.activeDragHandler.centroid.pressPosition.y * thumb.activeDragHandler.targetScale +
                                        thumb.activeDragHandler.centroid.position.y
                                width: cell.width * thumb.activeDragHandler.targetScale
                                height: cell.height * thumb.activeDragHandler.targetScale
                                opacity: thumb.activeDragHandler.targetOpacity
                            }
                        }
                    ]
                    transitions: Transition {
                        to: "normal"
                        enabled: heap.animationEnabled
                        NumberAnimation {
                            duration: heap.animationDuration
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
                    usesPlasmaTheme: false
                    width: PlasmaCore.Units.iconSizes.large
                    height: width
                    source: thumb.client.icon
                    anchors.horizontalCenter: thumbSource.horizontalCenter
                    anchors.bottom: thumbSource.bottom
                    anchors.bottomMargin: -height / 4
                    visible: !thumb.hidden && !activeDragHandler.active


                    PC3.Label {
                        id: caption
                        visible: heap.showCaptions
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
                    layout: expoLayout
                    enabled: !thumb.hidden
                    naturalX: thumb.client.x - targetScreen.geometry.x - expoLayout.Kirigami.ScenePosition.x
                    naturalY: thumb.client.y - targetScreen.geometry.y - expoLayout.Kirigami.ScenePosition.y
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
                            x: thumb.client.x - targetScreen.geometry.x - (heap.absolutePositioning ?  expoLayout.Kirigami.ScenePosition.x : 0)
                            y: thumb.client.y - targetScreen.geometry.y - (heap.absolutePositioning ?  expoLayout.Kirigami.ScenePosition.y : 0)
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
                            x: (thumb.client.x - targetScreen.geometry.x - (heap.absolutePositioning ?  expoLayout.Kirigami.ScenePosition.x : 0)) * (1 - effect.partialActivationFactor) + cell.x * effect.partialActivationFactor
                            y: (thumb.client.y - targetScreen.geometry.y - (heap.absolutePositioning ?  expoLayout.Kirigami.ScenePosition.y : 0)) * (1 - effect.partialActivationFactor) + cell.y * effect.partialActivationFactor
                            width: thumb.client.width * (1 - effect.partialActivationFactor) + cell.width * effect.partialActivationFactor
                            height: thumb.client.height * (1 - effect.partialActivationFactor) + cell.height * effect.partialActivationFactor
                            opacity: thumb.client.minimized ? effect.partialActivationFactor : 1
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
                    enabled: heap.animationEnabled
                    NumberAnimation {
                        duration: heap.animationDuration
                        properties: "x, y, width, height, opacity"
                        easing.type: Easing.InOutCubic
                    }
                }


                PlasmaCore.FrameSvgItem {
                    anchors.fill: parent
                    anchors.margins: -PlasmaCore.Units.smallSpacing
                    imagePath: "widgets/viewitem"
                    prefix: "hover"
                    z: -1
                    visible: !thumb.activeDragHandler.active && (hoverHandler.hovered || selected)
                }

                HoverHandler {
                    id: hoverHandler
                    onHoveredChanged: if (hovered != selected) {
                        heap.resetSelected();
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        KWinComponents.Workspace.activeClient = thumb.client;
                        heap.activated();
                    }
                }

                TapHandler {
                    acceptedPointerTypes: PointerDevice.GenericPointer | PointerDevice.Pen
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    onTapped: {
                        heap.windowClicked(thumb.client, eventPoint)
                    }
                }

                component DragManager : DragHandler {
                    id: dragHandler
                    target: null
                    grabPermissions: PointerHandler.CanTakeOverFromAnything

                    readonly property double targetScale: {
                        if (!heap.supportsDragUpGesture) {
                            return 1;
                        }
                        const localPressPosition = centroid.scenePressPosition.y - expoLayout.Kirigami.ScenePosition.y;
                        if (localPressPosition == 0) {
                            return 0.1
                        } else {
                            const localPosition = centroid.scenePosition.y - expoLayout.Kirigami.ScenePosition.y;
                            return Math.max(0.1, Math.min(localPosition / localPressPosition, 1))
                        }
                    }

                    onActiveChanged: {
                        heap.dragActive = active;
                        if (active) {
                            thumb.activeDragHandler = dragHandler;
                        } else {
                            thumbSource.Drag.drop();
                            let globalPos = targetScreen.mapToGlobal(centroid.scenePosition);
                            effect.checkItemDroppedOutOfScreen(globalPos, thumbSource);
                        }
                    }
                }
                property DragManager activeDragHandler: dragHandler
                DragManager {
                    id: dragHandler
                    readonly property double targetOpacity: 1
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                }
                DragManager {
                    id: touchDragHandler
                    acceptedDevices: PointerDevice.TouchScreen
                    readonly property double targetOpacity: {
                        if (!heap.supportsCloseWindows) {
                            return 1;
                        }
                        const startDistance = heap.Kirigami.ScenePosition.y + heap.height - centroid.scenePressPosition.y;
                        const localPosition = heap.Kirigami.ScenePosition.y + heap.height - centroid.scenePosition.y;
                        return Math.min(localPosition / startDistance, 1);
                    }

                    onActiveChanged: {
                        if (!active) {
                            if (heap.supportsCloseWindows && targetOpacity < 0.4) {
                                thumb.client.closeWindow();
                            }
                        }
                    }
                }

                PC3.Button {
                    id: closeButton
                    visible: heap.supportsCloseWindows && (hoverHandler.hovered || Kirigami.Settings.tabletMode || Kirigami.Settings.hasTransientTouchInput) && thumb.client.closeable && !dragHandler.active
                    anchors {
                        right: thumbSource.right
                        rightMargin: PlasmaCore.Units.smallSpacing
                        top: thumbSource.top
                        topMargin: PlasmaCore.Units.smallSpacing
                    }
                    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
                    icon.name: "window-close"
                    implicitWidth: PlasmaCore.Units.iconSizes.medium
                    implicitHeight: implicitWidth
                    onClicked: thumb.client.closeWindow();
                }

                Component.onDestruction: {
                    if (selected) {
                        heap.resetSelected();
                    }
                }
            }
        }
    }

    function findFirstItem() {
        for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
            const candidateItem = windowsRepeater.itemAt(candidateIndex);
            if (!candidateItem.hidden) {
                return candidateIndex;
            }
        }
        return -1;
    }

    function findNextItem(selectedIndex, direction) {
        if (selectedIndex == -1) {
            return findFirstItem();
        }

        const selectedItem = windowsRepeater.itemAt(selectedIndex);
        let nextIndex = -1;

        switch (direction) {
        case WindowHeap.Direction.Left:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width < selectedItem.x + selectedItem.width) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (candidateItem.x + candidateItem.width > nextItem.x + nextItem.width) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Right:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (selectedItem.x < candidateItem.x) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextIndex == -1 || candidateItem.x < nextItem.x) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Up:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height < selectedItem.y + selectedItem.height) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextItem.y + nextItem.height < candidateItem.y + candidateItem.height) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Down:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (selectedItem.y < candidateItem.y) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (candidateItem.y < nextItem.y) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        }

        return nextIndex;
    }

    function resetSelected() {
        heap.selectedIndex = -1;
    }

    function selectNextItem(direction) {
        const nextIndex = findNextItem(heap.selectedIndex, direction);
        if (nextIndex != -1) {
            heap.selectedIndex = nextIndex;
        }
    }

    function selectLastItem(direction) {
        let last = heap.selectedIndex;
        while (true) {
            const next = findNextItem(last, direction);
            if (next == -1) {
                break;
            } else {
                last = next;
            }
        }
        if (last != -1) {
            heap.selectedIndex = last;
        }
    }

    onActiveFocusChanged: resetSelected();

    Keys.onPressed: {
        switch (event.key) {
        case Qt.Key_Up:
            selectNextItem(WindowHeap.Direction.Up);
            break;
        case Qt.Key_Down:
            selectNextItem(WindowHeap.Direction.Down);
            break;
        case Qt.Key_Left:
            selectNextItem(WindowHeap.Direction.Left);
            break;
        case Qt.Key_Right:
            selectNextItem(WindowHeap.Direction.Right);
            break;
        case Qt.Key_Home:
            selectLastItem(WindowHeap.Direction.Left);
            break;
        case Qt.Key_End:
            selectLastItem(WindowHeap.Direction.Right);
            break;
        case Qt.Key_PageUp:
            selectLastItem(WindowHeap.Direction.Up);
            break;
        case Qt.Key_PageDown:
            selectLastItem(WindowHeap.Direction.Down);
            break;
        case Qt.Key_Return:
        case Qt.Key_Space:
            let selectedItem = null;
            if (heap.selectedIndex != -1) {
                selectedItem = windowsRepeater.itemAt(heap.selectedIndex);
            } else {
                // If the window heap has only one visible window, activate it.
                for (let i = 0; i < windowsRepeater.count; ++i) {
                    const candidateItem = windowsRepeater.itemAt(i);
                    if (candidateItem.hidden) {
                        continue;
                    } else if (selectedItem) {
                        selectedItem = null;
                        break;
                    }
                    selectedItem = candidateItem;
                }
            }
            if (selectedItem) {
                KWinComponents.Workspace.activeClient = selectedItem.client;
                heap.activated();
            }
            break;
        default:
            return;
        }
        event.accepted = true;
    }
}
