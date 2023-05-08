/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
    SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo@venerandi.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtGraphicalEffects 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.milou 0.3 as Milou
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras

FocusScope {
    id: container
    focus: true

    required property QtObject effect
    required property QtObject targetScreen

    readonly property bool lightBackground: Math.max(PlasmaCore.ColorScope.backgroundColor.r,
                                                     PlasmaCore.ColorScope.backgroundColor.g,
                                                     PlasmaCore.ColorScope.backgroundColor.b) > 0.5

    property bool animationEnabled: false
    property bool organized: false

    property bool verticalDesktopBar: desktopModel.gridLayoutRowCount >= bar.desktopCount && desktopModel.gridLayoutRowCount != 1
    property bool anyDesktopBar: verticalDesktopBar || desktopModel.gridLayoutRowCount == 1

    function start() {
        animationEnabled = true;
        organized = true;
        if (!effect.gestureInProgress) {
            effect.state = "overview"
        }
    }

    function stop() {
        organized = false;
        effect.state = "initial"
    }

    property bool gestureInProgress: effect.gestureInProgress
    onGestureInProgressChanged: {
        if (gestureInProgress) {
            if (effect.partialActivationFactor <= 0.5) {
                return
            }
            if (effect.state == "overview") {
                effect.state = "initial"
            } else if (effect.state == "grid") {
                effect.state = "overview"
            } else {
                effect.state = "grid"
            }

        } else {
            if (effect.partialActivationFactor <= 0.5) {
                if (effect.state == "initial") {
                    effect.quickDeactivate()
                }
                return
            }
            if (effect.state == "initial") {
                effect.state = "overview"
            } else if (effect.state == "overview") {
                effect.state = "grid"
            } else {
                effect.quickDeactivate()
                effect.state = "initial"
            }
        }
    }

    Keys.onEscapePressed: effect.deactivate();

    Keys.forwardTo: searchField

    Keys.onEnterPressed: {
        allDesktopHeaps.currentforceActiveFocus();
        if (allDesktopHeaps.current.count === 1) {
            allDesktopHeaps.current.activateCurrentClient();
        }
    }

    Keys.onLeftPressed: {
        let view = effect.getView(Qt.LeftEdge)
        if (view) {
            effect.activateView(view)
        }
    }
    Keys.onRightPressed: {
        let view = effect.getView(Qt.RightEdge)
        if (view) {
            effect.activateView(view)
        }
    }
    Keys.onUpPressed: {
        let view = effect.getView(Qt.TopEdge)
        if (view) {
            effect.activateView(view)
        }
    }
    Keys.onDownPressed: {
        let view = effect.getView(Qt.BottomEdge)
        if (view) {
            effect.activateView(view)
        }
    }

    KWinComponents.DesktopBackgroundItem {
        id: backgroundItem
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentVirtualDesktop
        outputName: targetScreen.name

        layer.enabled: effect.blurBackground
        layer.effect: FastBlur {radius: 64}
    }

    states: [
        State {
            name: "initial"
            when: effect.state == 'initial' && !effect.gestureInProgress
            PropertyChanges {
                target: container
                overviewVal: 0
                gridVal: 0
            }
        },
        State {
            name: "to-overview"
            when: effect.state == 'initial' && effect.gestureInProgress
            PropertyChanges {
                target: container
                overviewVal: effect.partialActivationFactor
                gridVal: 0
            }
        },
        State {
            name: "overview"
            when: effect.state == 'overview' && !effect.gestureInProgress
            PropertyChanges {
                target: container
                overviewVal: 1
                gridVal: 0
            }
        },
        State {
            name: "to-grid"
            when: effect.state == 'overview' && effect.gestureInProgress
            PropertyChanges {
                target: container
                overviewVal: 1 - effect.partialActivationFactor
                gridVal: effect.partialActivationFactor
            }
        },
        State {
            name: "grid"
            when: effect.state == 'grid' && !effect.gestureInProgress
            PropertyChanges {
                target: container
                overviewVal: 0
                gridVal: 1
            }
        },
        State {
            name: "to-initial"
            when: effect.state == 'grid' && effect.gestureInProgress
            PropertyChanges {
                target: container
                overviewVal: 0
                gridVal: 1 - effect.partialActivationFactor
            }
        }
    ]
    transitions: Transition {
        to: "initial, overview, grid"
        NumberAnimation {
            properties: "overviewVal, gridVal"
            duration: effect.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: underlay
        anchors.fill: parent
        opacity: 0.7
        color: PlasmaCore.ColorScope.backgroundColor

        TapHandler {
            onTapped: effect.deactivate();
        }
    }

    Item {
        id: desktopBar
        opacity: overviewVal
        visible: container.anyDesktopBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: container.verticalDesktopBar ? undefined : parent.right
        anchors.bottom: container.verticalDesktopBar ? parent.bottom : undefined
        height: bar.implicitHeight + 2 * PlasmaCore.Units.smallSpacing
        width: bar.implicitWidth + 2 * PlasmaCore.Units.smallSpacing

        DesktopBar {
            id: bar
            anchors.fill: parent
            clientModel: stackModel
            desktopModel: desktopModel
            verticalDesktopBar: container.verticalDesktopBar
            selectedDesktop: KWinComponents.Workspace.currentVirtualDesktop
            heap: allDesktopHeaps.current
        }
    }

    Item {
        id: topBar
        opacity: overviewVal
        anchors.left: container.verticalDesktopBar ? desktopBar.right : parent.left
        anchors.right: parent.right
        anchors.top: container.verticalDesktopBar || !container.anyDesktopBar ? parent.top : desktopBar.bottom
        height: searchField.height + 1 * PlasmaCore.Units.largeSpacing

        PlasmaExtras.SearchField {
            id: searchField
            anchors.centerIn: parent
            width: Math.min(parent.width, 20 * PlasmaCore.Units.gridUnit)
            focus: enabled
            readOnly: gridVal == 1
            onReadOnlyChanged: {
                text = ""
                effect.searchText = ""
                effect.searchTextChanged()
            }
            Keys.priority: Keys.BeforeItem
            Keys.forwardTo: text && allDesktopHeaps.current.count === 0 ? searchResults : allDesktopHeaps.current
            text: effect.searchText
            onTextEdited: {
                effect.searchText = text;
                allDesktopHeaps.current.resetSelected();
                allDesktopHeaps.current.selectNextItem(WindowHeap.Direction.Down);
                searchField.focus = true;
            }
        }
    }

    property var currentGeometry: targetScreen.geometry
    property int minX: PlasmaCore.Units.largeSpacing + (container.verticalDesktopBar ? desktopBar.width : 0)
    property int minY: PlasmaCore.Units.largeSpacing + topBar.height + (container.verticalDesktopBar || !container.anyDesktopBar ? 0 : desktopBar.height)
    property int maxWidth: currentGeometry.width - minX - PlasmaCore.Units.largeSpacing
    property int maxHeight: currentGeometry.height - minY - PlasmaCore.Units.largeSpacing
    property real maxRatio: maxWidth / maxHeight
    property real targetRatio: currentGeometry.width / currentGeometry.height
    property int targetHeight: maxRatio > targetRatio ? maxHeight : Math.round(maxWidth / targetRatio)
    property int targetWidth: maxRatio > targetRatio ? Math.round(maxHeight * targetRatio) : maxWidth
    property int targetX: Math.round(minX + (maxWidth - targetWidth) / 2)
    property int targetY: Math.round(minY + (maxHeight - targetHeight) / 2) // TODO probably now that I use transforms this could be simplified?

    property real overviewVal: 0
    property real gridVal: 0
    property int columns: Math.ceil(bar.desktopCount / rows)
    property int rows: desktopModel.gridLayoutRowCount

    Item {
        id: desktopGrid
        anchors.fill: parent
        property var dndManagerStore: ({})

        Repeater {
            id: allDesktopHeaps
            model: desktopModel

            property Item current
            property Item secondCurrent

            Kirigami.ShadowedRectangle {
                id: mainBackground
                color: "transparent"
                visible: gridVal > 0 || current || ((Math.abs(deltaColumn) <= 1 || Math.abs(deltaRow) <= 1) && !justCreated)
                anchors.fill: parent
                property bool shouldBeVisibleInOverview: !(container.organized && effect.searchText.length > 0 && current) || heap.count !== 0
                opacity: 1 - overviewVal * (shouldBeVisibleInOverview ? 0 : 1)

                required property QtObject desktop
                required property int index
                readonly property bool current: KWinComponents.Workspace.currentVirtualDesktop === desktop

                z: dragActive ? 1 : 0
                readonly property bool dragActive: heap.dragActive || dragHandler.active

                // The desktop might shuffle around as soon as it's
                // created since the rows/columns are updated after
                // the desktop is added. We don't want to see that.
                property bool justCreated: true
                Timer {
                    interval: effect.animationDuration
                    running: true
                    onTriggered: justCreated = false
                }

                shadow {
                    size: Kirigami.Units.gridUnit * 2
                    color: Qt.rgba(0, 0, 0, 0.3)
                    yOffset: 3
                }
                radius: Kirigami.Units.largeSpacing * (overviewVal + gridVal)

                property int gridSize: Math.max(rows, columns)
                property real row: (index - column) / columns
                property real column: index % columns
                property real deltaColumn: column - allDesktopHeaps.secondCurrent.column - effect.desktopOffset.x
                property real deltaRow: row - allDesktopHeaps.secondCurrent.row - effect.desktopOffset.y

                Behavior on deltaColumn {
                    enabled: overviewVal > 0
                    NumberAnimation {
                        duration: effect.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on deltaRow {
                    enabled: overviewVal > 0
                    NumberAnimation {
                        duration: effect.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }

                transform: [
                    // Scales down further, still in grid, to have some gaps between
                    // the desktops.
                    Scale {
                        origin.x: width / 2
                        origin.y: height / 2
                        xScale: 1 - 0.02 * gridVal
                        yScale: xScale
                    },
                    Scale {
                        id: gridScaling
                        xScale: 1 + (1 / gridSize - 1) * gridVal
                        yScale: xScale
                    },
                    // Further little translation in the grids to align the elements
                    // to the center of their cell
                    Translate{
                        x: (gridSize / columns - 1) * (width / gridSize) / 2 * gridVal
                        y: (gridSize / rows - 1) * (height / gridSize) / 2 * gridVal
                    },
                    // Scales down the desktops so that they do not overlap in the grid
                    // When in desktop grid, translates the desktop so that the whole
                    // grid fits in the view
                    Translate {
                        x: column * (width / columns) * gridVal
                        y: row * (height / rows) * gridVal
                    },
                    // Scales down the preview slighly when in Overview mode
                    Scale {
                        xScale: 1 + (targetWidth / width - 1) * overviewVal
                        yScale:1 + (targetHeight / height - 1) * overviewVal
                    },
                    // Initially places other desktops in a grid around the current one,
                    // and moves them slighly to avoid overlapping the UI
                    Translate {
                        x: targetX * overviewVal + deltaColumn * width * (1 - gridVal)
                        y: targetY * overviewVal + deltaRow * height * (1 - gridVal)
                    }
                ]

                KWinComponents.DesktopBackgroundItem {
                    id: desktopElement
                    anchors.fill: parent
                    activity: KWinComponents.Workspace.currentActivity
                    desktop: KWinComponents.Workspace.currentVirtualDesktop
                    outputName: targetScreen.name

                    layer.enabled: true
                    layer.effect: OpacityMask { // cached: true?
                        maskSource: Rectangle {
                            anchors.centerIn: parent
                            width: desktopElement.width
                            height: desktopElement.height
                            radius: mainBackground.radius
                        }
                    }
                }

                DropArea {
                    anchors.fill: parent
                    onEntered: {
                        drag.accepted = true;
                    }
                    onDropped: drop => {
                        drop.accepted = true;
                        if (drag.source instanceof Kirigami.ShadowedRectangle) {
                            // dragging a desktop as a whole
                            if (drag.source === mainBackground) {
                                drop.action = Qt.IgnoreAction;
                                return;
                            }
                            effect.swapDesktops(drag.source.desktop.x11DesktopNumber, desktop.x11DesktopNumber);
                        } else {
                            // dragging a KWin::Window
                            if (drag.source.desktop === desktop.x11DesktopNumber) {
                                drop.action = Qt.IgnoreAction;
                                return;
                            }
                            drag.source.desktop = desktop.x11DesktopNumber;
                        }
                    }
                }
                Connections {
                    target: effect
                    function onItemDroppedOutOfScreen(globalPos, item, screen) {
                        if (screen !== targetScreen) {
                            return;
                        }
                        const pos = screen.mapFromGlobal(globalPos);
                        if (!mainBackground.contains(mainBackground.mapFromItem(null, pos.x, pos.y))) {
                            return;
                        }
                        item.client.desktop = desktop.x11DesktopNumber;
                    }
                }
                DragHandler {
                    id: dragHandler
                    target: heap
                    grabPermissions: PointerHandler.ApprovesTakeOverByHandlersOfSameType
                    onActiveChanged: {
                        if (!active) {
                            heap.Drag.drop();
                            Qt.callLater(heap.resetPosition)
                        }
                    }
                }

                MouseArea {
                    anchors.fill: heap
                    acceptedButtons: Qt.NoButton
                    cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
                }

                WindowHeap {
                    id: heap
                    anchors.fill: parent

                    visible: true
                    function resetPosition() {
                        x = 0;
                        y = 0;
                    }
                    z: 9999
                    Drag.active: dragHandler.active
                    Drag.proposedAction: Qt.MoveAction
                    Drag.supportedActions: Qt.MoveAction
                    Drag.source: mainBackground
                    Drag.hotSpot: Qt.point(width * 0.5, height * 0.5)

                    layout.mode: effect.layout
                    focus: current
                    padding: PlasmaCore.Units.largeSpacing
                    animationDuration: effect.animationDuration
                    animationEnabled: container.animationEnabled && current
                    organized: container.organized
                    dndManagerStore: desktopGrid.dndManagerStore
                    Keys.priority: Keys.AfterItem
                    Keys.forwardTo: searchResults
                    model: KWinComponents.ClientFilterModel {
                        activity: KWinComponents.Workspace.currentActivity
                        desktop: mainBackground.desktop
                        screenName: targetScreen.name
                        clientModel: stackModel
                        filter: effect.searchText
                        minimizedWindows: !effect.ignoreMinimized
                        windowType: ~KWinComponents.ClientFilterModel.Dock &
                                    ~KWinComponents.ClientFilterModel.Desktop &
                                    ~KWinComponents.ClientFilterModel.Notification &
                                    ~KWinComponents.ClientFilterModel.CriticalNotification
                    }
                    delegate: WindowHeapDelegate {
                        windowHeap: heap
                        partialActivationFactor: overviewVal + gridVal

                        targetScale: {
                            if (!container.anyDesktopBar) return targetScale;
                            if (overviewVal != 1) return targetScale;
                            let coordinate = container.verticalDesktopBar ? 'x' : 'y'
                            if (!activeDragHandler.active) {
                                return targetScale; // leave it alone, so it won't affect transitions before they start
                            }
                            var localPressPosition = activeDragHandler.centroid.scenePressPosition[coordinate] - heap.layout.Kirigami.ScenePosition[coordinate];
                            if (localPressPosition === 0) {
                                return 0.1;
                            } else {
                                var localPosition = activeDragHandler.centroid.scenePosition[coordinate] - heap.layout.Kirigami.ScenePosition[coordinate];
                                return Math.max(0.1, Math.min(localPosition / localPressPosition, 1));
                            }
                        }

                        opacity: 1 - downGestureProgress
                        onDownGestureTriggered: client.closeWindow()
                    }
                    onActivated: effect.deactivate();
                    onWindowClicked: {
                        if (eventPoint.event.button === Qt.MiddleButton) {
                            window.closeWindow();
                        }
                    }
                }
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        KWinComponents.Workspace.currentVirtualDesktop = mainBackground.desktop;
                        container.effect.deactivate(container.effect.animationDuration);
                    }
                }
                onCurrentChanged: {
                    if (current) {
                        allDesktopHeaps.current = heap;
                        allDesktopHeaps.secondCurrent = mainBackground;
                    }
                }
                Component.onCompleted: {
                    if (current) {
                        allDesktopHeaps.current = heap;
                        allDesktopHeaps.secondCurrent = mainBackground;
                    }
                }
            }

        }

    }

    Column {
        anchors.left: container.verticalDesktopBar ? desktopBar.right : parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: topBar.bottom

        Item {
            width: parent.width
            height: parent.height - topBar.height
            visible: container.organized && effect.searchText.length > 0 && allDesktopHeaps.current.count === 0
            opacity: overviewVal

            PlasmaExtras.PlaceholderMessage {
                id: placeholderMessage
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                text: i18nd("kwin_effects", "No matching windows")
            }

            Milou.ResultsView {
                id: searchResults
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: parent.height - placeholderMessage.height - PlasmaCore.Units.largeSpacing
                queryString: effect.searchText

                onActivated: {
                    effect.deactivate();
                }
            }
        }
    }

    Repeater {
        model: KWinComponents.ClientFilterModel {
            desktop: KWinComponents.Workspace.currentVirtualDesktop
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: KWinComponents.ClientFilterModel.Dock
        }

        KWinComponents.WindowThumbnailItem {
            id: windowThumbnail
            visible: !model.client.hidden && opacity > 0
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            z: model.client.stackingOrder
            width: model.client.width
            height: model.client.height
            opacity: 1 - (gridVal + overviewVal)
        }
    }

    KWinComponents.ClientModel {
        id: stackModel
    }

    KWinComponents.VirtualDesktopModel {
        id: desktopModel
    }

    Component.onCompleted: {
        // The following line unbinds the verticalDesktopBar, meaning that it
        // won't react to changes in number of desktops or rows. This is beacuse we
        // don't want the desktop bar changing screenside whilst the user is
        // interacting with it, e.g. by adding desktops
        container.verticalDesktopBar = container.verticalDesktopBar
        start();
    }
}
