/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
    SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo@venerandi.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.milou as Milou
import org.kde.plasma.components as PC3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kcmutils as KCM

FocusScope {
    id: container
    focus: true

    readonly property QtObject effect: KWinComponents.SceneView.effect
    readonly property QtObject targetScreen: KWinComponents.SceneView.screen

    readonly property bool lightBackground: Math.max(Kirigami.Theme.backgroundColor.r,
                                                     Kirigami.Theme.backgroundColor.g,
                                                     Kirigami.Theme.backgroundColor.b) > 0.5

    property bool animationEnabled: false
    property bool organized: false

    property bool verticalDesktopBar: KWinComponents.Workspace.desktopGridHeight >= bar.desktopCount && KWinComponents.Workspace.desktopGridHeight != 1

    // The values of overviewVal and gridVal might not be 0 on startup,
    // but we always want to animate from 0 to those values. So, we initially
    // always set them to 0 and bind their full values when the effect starts.
    // See start()
    property real overviewVal: 0
    property real gridVal: 0

    states: [
        State {
            name: "initial"
            PropertyChanges {
                target: container
                overviewVal: 0
                gridVal: 0
            }
        },
        State {
            name: "grid"
            PropertyChanges {
                target: container
                overviewVal: 0
                gridVal: 1
            }
        },
        State {
            name: "overview"
            PropertyChanges {
                target: container
                overviewVal: 1
                gridVal: 0
            }
        },
        State {
            name: "partialOverview"
            PropertyChanges {
                target: container
                overviewVal: effect.overviewPartialActivationFactor
                gridVal: 0
            }
        },
        State {
            name: "partialGrid"
            PropertyChanges {
                target: container
                overviewVal: 0
                gridVal: effect.gridPartialActivationFactor
            }
        },
        State {
            name: "transition"
            PropertyChanges {
                target: container
                overviewVal: 1 - effect.transitionPartialActivationFactor
                gridVal: effect.transitionPartialActivationFactor
            }
        }
    ]
    state: {
        // If the effect hasn't started, we remain on the initial state
        if (!organized) return "initial";

        // If a gesture is ongoing, we use a partial state
        if (effect.overviewGestureInProgress) return "partialOverview";
        if (effect.gridGestureInProgress) return "partialGrid";
        if (effect.transitionGestureInProgress) return "transition";

        // If either the overview or grid gestures are completed, either
        // by a touchpad/touchscreen gesture or through a shortcut, we use
        // that state
        if (effect.overviewPartialActivationFactor === 1) return "overview";
        if (effect.gridPartialActivationFactor === 1) return "grid";

        // If neither is active, we are in the initial state.
        if (effect.overviewPartialActivationFactor + effect.gridPartialActivationFactor === 0) return "initial";

        // If we are in between a state but no gesture is going on, we snap to
        // the closest state
        if (overviewVal >= 0.5) return "overview";
        if (gridVal >= 0.5) return "grid";
        return "initial";
    }
    transitions: Transition {
        to: "initial, grid, overview"
        NumberAnimation {
            duration: effect.animationDuration
            properties: "gridVal, overviewVal"
            easing.type: Easing.OutCubic
        }
    }

    function switchTo(desktop) {
        KWinComponents.Workspace.currentDesktop = desktop;
        effect.deactivate();
    }

    function selectNext(direction) {
        if (effect.searchText !== "") return false;
        let currentIndex = 0
        for (let i = 0; i < allDesktopHeaps.count; i++) {
            if (allDesktopHeaps.itemAt(i).current) {
                currentIndex = i;
                break;
            }
        }
        let x = currentIndex % container.columns;
        let y = Math.floor(currentIndex / container.columns);

        // the direction we move in is the opposite of the window to select
        // i.e pressing left should select the rightmost window on the desktop
        // to the left
        let invertedDirection;
        switch(direction) {
            case WindowHeap.Direction.Up:
                y--;
                invertedDirection = WindowHeap.Direction.Down;
                break;
            case WindowHeap.Direction.Down:
                y++
                invertedDirection = WindowHeap.Direction.Up;
                break;
            case WindowHeap.Direction.Left:
                x--;
                invertedDirection = WindowHeap.Direction.Right;
                break;
            case WindowHeap.Direction.Right:
                x++;
                invertedDirection = WindowHeap.Direction.Left;
                break;
        }

        if (x < 0 || x >= container.columns) {
            return false;
        }
        if (y < 0 || y >= container.rows) {
            return false;
        }
        let newIndex = y * container.columns + x;

        KWinComponents.Workspace.currentDesktop = allDesktopHeaps.itemAt(newIndex).desktop
        allDesktopHeaps.itemAt(newIndex).nestedHeap.focus = true
        allDesktopHeaps.itemAt(newIndex).selectLastItem(invertedDirection);
        return true;
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) {
            if (effect.searchText !== "") {
                effect.searchText = ""
            } else {
                effect.deactivate();
            }
        } else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            desktopModel.create(desktopModel.rowCount());
        } else if (event.key === Qt.Key_Minus) {
            desktopModel.remove(desktopModel.rowCount() - 1);
        } else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) {
            const desktopId = event.key - Qt.Key_F1;
            if (desktopId < allDesktopHeaps.count) {
                switchTo(allDesktopHeaps.itemAt(desktopId).desktop);
            }
        } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            const desktopId = event.key === Qt.Key_0 ? 9 : (event.key - Qt.Key_1);
            if (desktopId < allDesktopHeaps.count) {
                switchTo(allDesktopHeaps.itemAt(desktopId).desktop);
            }
        } else if (event.key === Qt.Key_Up) {
            event.accepted = selectNext(WindowHeap.Direction.Up);
            if (!event.accepted) {
                let view = effect.getView(Qt.TopEdge)
                if (view) {
                    effect.activateView(view)
                }
            }
        } else if (event.key === Qt.Key_Down) {
            event.accepted = selectNext(WindowHeap.Direction.Down);
            if (!event.accepted) {
                let view = effect.getView(Qt.BottomEdge)
                if (view) {
                    effect.activateView(view)
                }
            }
        } else if (event.key === Qt.Key_Left) {
            event.accepted = selectNext(WindowHeap.Direction.Left);
            if (!event.accepted) {
                let view = effect.getView(Qt.LeftEdge)
                if (view) {
                    effect.activateView(view)
                }
            }
        } else if (event.key === Qt.Key_Right) {
            event.accepted = selectNext(WindowHeap.Direction.Right);
            if (!event.accepted) {
                let view = effect.getView(Qt.RightEdge)
                if (view) {
                    effect.activateView(view)
                }
            }
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
            for (let i = 0; i < allDesktopHeaps.count; i++) {
                if (allDesktopHeaps.itemAt(i).current) {
                    switchTo(allDesktopHeaps.itemAt(i).desktop)
                    break;
                }
            }
        }
    }
    Keys.priority: Keys.AfterItem
    Keys.forwardTo: [allDesktopHeaps.currentHeap, searchField, searchResults]

    Item {
        width: backgroundItem.width
        height: backgroundItem.height
        KWinComponents.DesktopBackground {
            id: backgroundItem
            activity: KWinComponents.Workspace.currentActivity
            desktop: KWinComponents.Workspace.currentDesktop
            outputName: targetScreen.name
            visible: false
        }
        FastBlur {
            anchors.fill: parent
            source: backgroundItem
            radius: 64
        }
    }

    Rectangle {
        id: underlay
        anchors.fill: parent
        opacity: 0.7
        color: Kirigami.Theme.backgroundColor

        TapHandler {
            onTapped: effect.deactivate();
        }
    }

    Item {
        id: desktopBar

        // (overviewVal, gridVal) represents the state of the overview
        // in a 2D coordinate plane. Math.atan2 returns the angle between
        // the x axis and that point. By using this to set the opacity,
        // we can have an opaque desktopBar when the point moves from
        // the origin to the "overviewVal" direction, which has angle pi/2,
        // and a transparent desktopBar when the point moves from
        // the origin to the "gridVal" direction, which has angle 0,
        // whilst still animating when the point moves from overviewVal to
        // gridVal too.
        opacity: overviewVal * Math.atan2(overviewVal, gridVal) / Math.PI * 2

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: container.verticalDesktopBar ? undefined : parent.right
        anchors.bottom: container.verticalDesktopBar ? parent.bottom : undefined
        height: bar.implicitHeight + 2 * Kirigami.Units.smallSpacing
        width: bar.implicitWidth + 2 * Kirigami.Units.smallSpacing

        DesktopBar {
            id: bar
            anchors.fill: parent
            windowModel: stackModel
            desktopModel: desktopModel
            verticalDesktopBar: container.verticalDesktopBar
            selectedDesktop: KWinComponents.Workspace.currentDesktop
            heap: allDesktopHeaps.currentHeap
        }
    }

    Item {
        id: topBar
        opacity: desktopBar.opacity
        anchors.left: container.verticalDesktopBar ? desktopBar.right : parent.left
        anchors.right: parent.right
        anchors.top: container.verticalDesktopBar ? parent.top : desktopBar.bottom
        anchors.topMargin: Kirigami.Units.largeSpacing
        height: searchField.height + 1 * Kirigami.Units.largeSpacing

        PlasmaExtras.SearchField {
            id: searchField
            anchors.centerIn: parent
            width: Math.min(parent.width, 20 * Kirigami.Units.gridUnit)
            focus: enabled
            readOnly: gridVal == 1
            onReadOnlyChanged: {
                text = ""
                effect.searchText = ""
                effect.searchTextChanged()
            }
            text: effect.searchText
            // not onTextEdited so that the UI always stays in sync
            onTextChanged: {
                effect.searchText = text;
                allDesktopHeaps.currentHeap.resetSelected();
                allDesktopHeaps.currentHeap.selectNextItem(WindowHeap.Direction.Down);
                searchField.focus = true;
            }
        }
    }

    property var currentGeometry: targetScreen.geometry

    // These are the minimum position of maximum size of the desktop preview in the overview
    property int minX: Kirigami.Units.largeSpacing + (container.verticalDesktopBar ? desktopBar.width : 0)
    property int minY: Kirigami.Units.largeSpacing + topBar.height + (container.verticalDesktopBar ? 0 : desktopBar.height)
    property int maxWidth: currentGeometry.width - minX - Kirigami.Units.gridUnit * 2
    property int maxHeight: currentGeometry.height - minY - Kirigami.Units.gridUnit * 2

    property int desktops: Math.max(bar.desktopCount, 2)
    property int columns: Math.ceil(desktops / rows)
    property int rows: KWinComponents.Workspace.desktopGridHeight

    // The desktop might shuffle around as soon as it's
    // created since the rows/columns are updated after
    // the desktop is added. We don't want to see that.
    property bool desktopJustCreated: false
    onDesktopsChanged: {
        desktopJustCreated = true
        startTimer.running = true
    }

    Timer {
        id: startTimer
        interval: effect.animationDuration
        running: false
        onTriggered: desktopJustCreated = false
    }

    Item {
        id: desktopGrid
        anchors.fill: parent
        property var dndManagerStore: ({})

        ColumnLayout {
            x: Math.round(parent.width / 2) + Math.round(parent.width / 8)
            width: Math.round(parent.width / 2) - Math.round(parent.width / 8) * 2
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.desktopCount === 1
            opacity: gridVal
            spacing: Kirigami.Units.largeSpacing

            Kirigami.PlaceholderMessage {
                text: i18ndc("kwin", "@info:placeholder", "No other Virtual Desktops to show")
                icon.name: "virtual-desktops-symbolic"
            }

            PC3.Button {
                Layout.alignment: Qt.AlignHCenter
                text: i18ndc("kwin", "@action:button", "Add Virtual Desktop")
                icon.name: "list-add-symbolic"
                onClicked: desktopModel.create(desktopModel.rowCount())
            }

            PC3.Button {
                Layout.alignment: Qt.AlignHCenter
                text: i18ndc("kwin", "@action:button", "Configure Virtual Desktops…")
                icon.name: "preferences-virtual-desktops"
                onClicked: {
                    KCM.KCMLauncher.openSystemSettings("kcm_kwin_virtualdesktops")
                    effect.deactivate();
                }
            }

        }


        Repeater {
            id: allDesktopHeaps
            model: desktopModel

            property Item currentHeap
            property Item currentBackgroundItem

            Item {
                id: mainBackground

                // visible when in the overview 'grid' mode, but also keep neighbouring items visible as they can appear during gestures
                visible: gridVal > 0 || nearCurrent
                // Avoid handling drops for offscreen visible items
                enabled: gridVal > 0 || current

                anchors.fill: parent
                property bool shouldBeVisibleInOverview: !(effect.searchText.length > 0 && current) || (heap.count !== 0 && effect.filterWindows)
                opacity: 1 - overviewVal * (shouldBeVisibleInOverview ? 0 : 1)

                function selectLastItem(direction) {
                    heap.selectLastItem(direction);
                }


                required property QtObject desktop
                required property int index
                readonly property bool current: KWinComponents.Workspace.currentDesktop === desktop
                readonly property bool nearCurrent: Math.abs(deltaColumn) <= 1 && Math.abs(deltaRow) <= 1
                readonly property var nestedHeap: heap

                z: dragActive ? 1 : 0
                readonly property bool dragActive: heap.dragActive || dragHandler.active

                property int gridSize: Math.max(rows, columns)
                property real row: (index - column) / columns
                property real column: index % columns
                // deltaX and deltaY are used to move all the desktops together to 1:1 animate the
                // switching between different desktops
                property real deltaX: (!current ? effect.desktopOffset.x :
                                       column == 0 ? Math.max(0, effect.desktopOffset.x) :
                                       column == columns - 1 ? Math.min(0, effect.desktopOffset.x) :
                                       effect.desktopOffset.x)
                property real deltaY: (!current ? effect.desktopOffset.y :
                                       row == 0 ? Math.max(0, effect.desktopOffset.y) :
                                       row == rows - 1 ? Math.min(0, effect.desktopOffset.y) :
                                       effect.desktopOffset.y)
                // deltaColumn and deltaRows are the difference between the column/row of this desktop
                // compared to the column/row of the active one
                property real deltaColumn: column - allDesktopHeaps.currentBackgroundItem.column - deltaX
                property real deltaRow: row - allDesktopHeaps.currentBackgroundItem.row - deltaY

                onDeltaColumnChanged: heap.layout.updateCellsMapping()
                onDeltaRowChanged: heap.layout.updateCellsMapping()

                Behavior on deltaColumn {
                    enabled: overviewVal > 0 && !container.desktopJustCreated
                    NumberAnimation {
                        duration: effect.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on deltaRow {
                    enabled: overviewVal > 0 && !container.desktopJustCreated
                    NumberAnimation {
                        duration: effect.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }

                // Note that transforms should be read from the last one to the first one
                transform: [
                    // Scales down further, still in grid, to have some gaps between
                    // the desktops.
                    Scale {
                        origin.x: width / 2
                        origin.y: height / 2
                        xScale: 1 - 0.02 * gridVal
                        yScale: xScale
                    },
                    // Scales down the desktops so that they do not overlap in the grid
                    Scale {
                        id: gridScale
                        xScale: 1 + (1 / gridSize - 1) * gridVal
                        yScale: xScale
                    },
                    // Further little translation in the grids to align the elements
                    // to the center of their cell
                    Translate{
                        x: (gridSize / columns - 1) * (width / gridSize) / 2 * gridVal
                        y: (gridSize / rows - 1) * (height / gridSize) / 2 * gridVal
                    },
                    // When in desktop grid, translates the desktop so that the whole
                    // grid fits in the view
                    Translate {
                        x: column * (width / columns) * gridVal
                        y: row * (height / rows) * gridVal
                    },
                    // Initially places transition desktops in a grid around the current one,
                    // and moves them slightly to avoid overlapping the UI
                    Translate {
                        id: desktopTranslation
                        x: minX * 0.5 * overviewVal + deltaColumn * width * (1 - gridVal)
                        y: minY * 0.5 * overviewVal + deltaRow * height * (1 - gridVal)
                    }
                ]

                Item {
                    id: backgroundArea
                    property real sizeAdjust: Math.min(maxWidth / parent.width, maxHeight / parent.height)
                    width: parent.width * (1 + (sizeAdjust - 1) * overviewVal)
                    height: parent.height * (1 + (sizeAdjust - 1) * overviewVal)
                    x: parent.width / 2 - width / 2
                    y: parent.height / 2 - height / 2
                    KWinComponents.DesktopBackground {
                        id: desktopElement
                        anchors.fill: parent
                        anchors.margins: gridVal !== 0 ? Math.round(mainBackground.current * gridVal * (1.5 / gridScale.xScale)) : 0
                        activity: KWinComponents.Workspace.currentActivity
                        desktop: KWinComponents.Workspace.currentDesktop
                        outputName: targetScreen.name
                        visible: false
                    }

                    Kirigami.ShadowedTexture {
                        anchors.fill: parent

                        color: "transparent"
                        source: desktopElement

                        radius: Kirigami.Units.cornerRadius * (overviewVal + gridVal * 2)

                        shadow {
                            size: Kirigami.Units.gridUnit * 2
                            color: Qt.rgba(0, 0, 0, 0.3)
                            yOffset: 3
                        }

                        property var borderWidth: desktopHover.hovered ? Kirigami.Units.largeSpacing : (mainBackground.current ? Kirigami.Units.smallSpacing : 0);
                        property var borderColor: desktopHover.hovered ? Kirigami.Theme.focusColor : (mainBackground.current ? Kirigami.Theme.highlightColor : "transparent")
                        border.width: gridVal == 1 ? borderWidth : 0
                        border.color: gridVal == 1 ? borderColor : "transparent"
                    }

                    DropArea {
                        anchors.fill: parent
                        onEntered: (drag) => {
                            drag.accepted = true;
                        }
                        onDropped: (drop) => {
                            drop.accepted = true;
                            if (drop.keys.includes("kwin-desktop")) {
                                // dragging a desktop as a whole
                                if (drag.source === mainBackground) {
                                    drop.action = Qt.IgnoreAction;
                                    return;
                                }
                                KWinComponents.Workspace.moveDesktop(drag.source.desktop, desktop.x11DesktopNumber - 1);
                            } else {
                                // dragging a KWin::Window
                                if (drag.source.desktops.length === 0 || drag.source.desktops.indexOf(mainBackground.desktop) !== -1) {
                                    drop.action = Qt.IgnoreAction;
                                    return;
                                }
                                drag.source.desktops = [mainBackground.desktop];
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
                            // moving the window to the client screen is handled by WindowHeap's similarly named function that is also run
                            item.client.desktops = [mainBackground.desktop];
                        }
                    }
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: {
                            KWinComponents.Workspace.currentDesktop = mainBackground.desktop;
                            container.effect.deactivate();
                        }
                    }
                    DragHandler {
                        id: dragHandler
                        target: heap
                        enabled: gridVal !== 0
                        grabPermissions: PointerHandler.ApprovesTakeOverByHandlersOfSameType
                        onActiveChanged: {
                            if (!active) {
                                heap.Drag.drop();
                                Qt.callLater(heap.resetPosition)
                            }
                        }
                    }

                    HoverHandler {
                        id: desktopHover
                    }
                }

                WindowHeap {
                    id: heap
                    visible: !effect.filterWindows ? effect.searchText.length === 0 : true
                    width: parent.width * (1 + (backgroundArea.sizeAdjust - 1))
                    height: parent.height * (1 + (backgroundArea.sizeAdjust - 1))
                    x: parent.width / 2 - width / 2
                    y: parent.height / 2 - height / 2

                    function resetPosition() {
                        x = parent.width / 2 - width / 2;
                        y = parent.height / 2 - height / 2;
                    }
                    z: 9999
                    Drag.active: dragHandler.active
                    Drag.proposedAction: Qt.MoveAction
                    Drag.supportedActions: Qt.MoveAction
                    Drag.source: mainBackground
                    Drag.hotSpot: Qt.point(width * 0.5, height * 0.5)
                    Drag.keys: ["kwin-desktop"]

                    focus: current
                    padding: Kirigami.Units.largeSpacing
                    animationDuration: effect.animationDuration
                    animationEnabled:  (gridVal !== 0 || mainBackground.current) && organized
                    organized: container.state !== "initial"
                    dndManagerStore: desktopGrid.dndManagerStore
                    model: KWinComponents.WindowFilterModel {
                        activity: KWinComponents.Workspace.currentActivity
                        desktop: mainBackground.desktop
                        screenName: targetScreen.name
                        windowModel: stackModel
                        filter: effect.searchText
                        minimizedWindows: !effect.ignoreMinimized
                        windowType: ~KWinComponents.WindowFilterModel.Dock &
                                    ~KWinComponents.WindowFilterModel.Desktop &
                                    ~KWinComponents.WindowFilterModel.Notification &
                                    ~KWinComponents.WindowFilterModel.CriticalNotification
                    }
                    delegate: WindowHeapDelegate {
                        windowHeap: heap
                        offsetX: mainBackground.deltaColumn * container.width * (gridVal > 0 ? 0 : 1) + (dragHandler.active ? (dragHandler.centroid.pressPosition.x - dragHandler.centroid.position.x) : 0)
                        offsetY: mainBackground.deltaRow * container.height * (1 - gridVal) + (dragHandler.active ? (dragHandler.centroid.pressPosition.y - dragHandler.centroid.position.y) : 0)

                        partialActivationFactor: container.overviewVal + container.gridVal * effect.organizedGrid
                        // Parent switch needed for the option "organize windows in gridview"  to work correctly
                        contentItemParent: container.gridVal > 0 ? mainBackground : container

                        // This is preferable over using gestureInProgress values since gridVal and
                        // overviewVal are animated even after the gesture ends, and since the partial
                        // activation factor follows those two values, this results in a more
                        // fluent animation.
                        gestureInProgress: !Number.isInteger(gridVal) || !Number.isInteger(overviewVal)

                        targetScale: {
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
                        onDownGestureTriggered: window.closeWindow()
                        TapHandler {
                            acceptedPointerTypes: PointerDevice.Generic | PointerDevice.Pen
                            acceptedButtons: Qt.MiddleButton | Qt.RightButton
                            onTapped: (eventPoint, button) => {
                                if (button === Qt.MiddleButton) {
                                    window.closeWindow();
                                } else if (button === Qt.RightButton) {
                                    if (window.desktops.length > 0) {
                                        window.desktops = [];
                                    } else {
                                        window.desktops = [mainBackground.desktop];
                                    }
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        acceptedButtons: Qt.NoButton
                        cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
                    }

                    onActivated: effect.deactivate()
                }

                onCurrentChanged: {
                    if (current) {
                        allDesktopHeaps.currentHeap = heap;
                        allDesktopHeaps.currentBackgroundItem = mainBackground;
                    }
                }
                Component.onCompleted: {
                    if (current) {
                        allDesktopHeaps.currentHeap = heap;
                        allDesktopHeaps.currentBackgroundItem = mainBackground;
                    }
                    startTimer.running = true
                }
            }

        }

    }

    Loader {
        id: searchResults
        active: effect.searchText.length > 0 && (allDesktopHeaps.currentHeap.count === 0 || !effect.filterWindows)
        anchors.left: container.verticalDesktopBar ? desktopBar.right : parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: topBar.bottom

        sourceComponent: Item {
            width: parent.width
            height: parent.height - topBar.height
            opacity: overviewVal

            PlasmaExtras.PlaceholderMessage {
                id: placeholderMessage
                visible: allDesktopHeaps.currentHeap.count === 0 && effect.filterWindows
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                text: i18ndc("kwin", "@info:placeholder", "No matching windows")
            }

            Milou.ResultsView {
                id: milouView
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: effect.filterWindows ? parent.height - placeholderMessage.height - Kirigami.Units.largeSpacing : parent.height - Kirigami.Units.largeSpacing
                queryString: effect.searchText

                onActivated: {
                    effect.deactivate();
                }
            }

            Keys.forwardTo: milouView
        }

        Keys.forwardTo: item
    }

    Repeater {
        model: KWinComponents.WindowFilterModel {
            desktop: KWinComponents.Workspace.currentDesktop
            screenName: targetScreen.name
            windowModel: stackModel
            windowType: KWinComponents.WindowFilterModel.Dock
        }

        KWinComponents.WindowThumbnail {
            id: windowThumbnail
            visible: !model.window.hidden && opacity > 0
            wId: model.window.internalId
            x: model.window.x - targetScreen.geometry.x
            y: model.window.y - targetScreen.geometry.y
            z: model.window.stackingOrder
            width: model.window.width
            height: model.window.height
            opacity: 1 - (gridVal + overviewVal)
        }
    }

    KWinComponents.WindowModel {
        id: stackModel
    }

    KWinComponents.VirtualDesktopModel {
        id: desktopModel
    }

    Component.onCompleted: {
        // The following line unbinds the verticalDesktopBar, meaning that it
        // won't react to changes in number of desktops or rows. This is because we
        // don't want the desktop bar changing screenside whilst the user is
        // interacting with it, e.g. by adding desktops
        container.verticalDesktopBar = container.verticalDesktopBar
        organized = true
    }
}
