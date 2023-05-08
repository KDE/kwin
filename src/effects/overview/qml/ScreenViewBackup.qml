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
        effect.state = "overview"
    }

    function stop() {
        organized = false;
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

    state: effect.state

    states: [
        State {
            name: "initial"
            PropertyChanges {
                target: container
                initialVal: 1
                overviewVal: 0
                gridVal: 0
            }
        },
        State {
            name: "partial"
        },
        State {
            name: "overview"
            PropertyChanges {
                target: container
                initialVal: 0
                overviewVal: 1
                gridVal: 0
            }
        },
        State {
            name: "grid"
            PropertyChanges {
                target: container
                initialVal: 0
                overviewVal: 0
                gridVal: 1
            }
            PropertyChanges {
                target: topBar
                opacity: 0
            }
            PropertyChanges {
                target: desktopBar
                opacity: 0
            }
        }
    ]
    transitions: Transition {
        to: "initial, overview, grid"
        NumberAnimation {
            properties: "initialVal, overviewVal, gridVal, opacity"
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
        anchors.left: container.verticalDesktopBar ? desktopBar.right : parent.left
        anchors.right: parent.right
        anchors.top: container.verticalDesktopBar || !container.anyDesktopBar ? parent.top : desktopBar.bottom
        height: searchField.height + 2 * PlasmaCore.Units.largeSpacing

        PlasmaExtras.SearchField {
            id: searchField
            anchors.centerIn: parent
            width: Math.min(parent.width, 20 * PlasmaCore.Units.gridUnit)
            focus: true
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
    property int targetY: Math.round(minY + (maxHeight - targetHeight) / 2)

    property real initialVal: 0
    property real overviewVal: 0
    property real gridVal: 0
    property int columns: 2
    property int rows: desktopModel.gridLayoutRowCount

    Item {
        anchors.fill: parent

        Repeater {
            id: allDesktopRects
            model: desktopModel

            property Item current

            Kirigami.ShadowedRectangle {
                id: mainBackground
                color: "transparent"
                visible: gridVal > 0 || current

                function threePointInterpolate(start, middle, end, x, y, z) {
                    return start * x + middle * y + end * z
                }

                required property QtObject desktop
                required property int index
                readonly property bool current: KWinComponents.Workspace.currentVirtualDesktop === desktop

                property int column: (index - row) / desktopModel.gridLayoutRowCount
                property int row: index % desktopModel.gridLayoutRowCount

                property int deltaColumn: column - allDesktopRects.current.column
                property int deltaRow: row - allDesktopRects.current.row

                x: 0
                y: 0
                width: targetScreen.geometry.width
                height: targetScreen.geometry.height

                transform: [
                    Scale {
                        xScale: 1 + (targetWidth / targetScreen.geometry.width - 1) * overviewVal
                        yScale:1 + (targetHeight / targetScreen.geometry.height - 1) * overviewVal
                    },
                    Translate {
                        x: targetX * overviewVal
                        y: targetY * overviewVal
                    }
                ]

                //x: threePointInterpolate(0 + deltaRow * targetScreen.geometry.width,
                //               targetX + deltaRow * targetScreen.geometry.width,
                //               targetScreen.geometry.width / rows * row,
                //               initialVal, overviewVal, gridVal)
                //y: threePointInterpolate(0 + deltaColumn * targetScreen.geometry.height,
                //               targetY + deltaColumn * targetScreen.geometry.height,
                //               targetScreen.geometry.height / columns * column,
                //               initialVal, overviewVal, gridVal)
                //width: threePointInterpolate(targetScreen.geometry.width,
                //                   targetWidth,
                //                   targetScreen.geometry.width / rows,
                //                   initialVal, overviewVal, gridVal)
                //height: threePointInterpolate(targetScreen.geometry.height,
                //                    targetHeight,
                //                    targetScreen.geometry.height / columns,
                //                    initialVal, overviewVal, gridVal)
                //scale: threePointInterpolate(1, 1, 0.98, initialVal, overviewVal, gridVal)

                KWinComponents.DesktopBackgroundItem {
                    id: desktopElement
                    anchors.fill: parent
                    activity: KWinComponents.Workspace.currentActivity
                    desktop: KWinComponents.Workspace.currentVirtualDesktop
                    outputName: targetScreen.name

                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            anchors.centerIn: parent
                            width: desktopElement.width
                            height: desktopElement.height
                            radius: mainBackground.radius
                        }
                    }
                }
                onCurrentChanged: {
                    if (current) {
                        allDesktopRects.current = mainBackground;
                    }
                }
                Component.onCompleted: {
                    if (current) {
                        allDesktopRects.current = mainBackground;
                    }
                }

                shadow {
                    size: Kirigami.Units.gridUnit * 2
                    color: Qt.rgba(0, 0, 0, 0.3)
                    yOffset: 3
                }
                radius: Kirigami.Units.smallSpacing * 2
            }

        }

        /*Repeater {
            model: desktopModel
            id: allDesktopHeaps

            property Item current

            WindowHeap {
                id: heap
                visible: gridVal > 0 || (current && (!(container.organized && effect.searchText.length > 0) || heap.count !== 0))

                required property QtObject desktop
                required property int index
                readonly property bool current: KWinComponents.Workspace.currentVirtualDesktop === desktop

                property int column: (index - row) / desktopModel.gridLayoutRowCount
                property int row: index % desktopModel.gridLayoutRowCount

                property int deltaColumn: column - allDesktopHeaps.current.column
                property int deltaRow: row - allDesktopHeaps.current.row

                property int initialX: targetX + deltaRow * targetScreen.geometry.width
                property int initialY: targetY + deltaColumn * targetScreen.geometry.height

                x: initialX
                y: initialY
                width: targetWidth
                height: targetHeight
                transform: [
                    Scale {
                        origin.x: width / 2
                        origin.y: height / 2
                        xScale: 1 - 0.12 * overviewVal
                        yScale: 1 - 0.12 * overviewVal
                    },
                    Scale {
                        origin.x: 0
                        origin.y: 0
                        xScale: 1 - (1 - 1 / rows) * (targetWidth / targetScreen.geometry.width) * gridVal
                        yScale: 1 - (1 - 1 / columns) * (targetHeight / targetScreen.geometry.height) * gridVal
                    },
                    Translate {
                        x: (row * (targetScreen.geometry.width / rows) - initialX) * gridVal
                        y: (column * (targetScreen.geometry.height / columns) - initialY) * gridVal
                    }
                ]

                layout.mode: effect.layout
                focus: current
                padding: PlasmaCore.Units.largeSpacing
                animationDuration: effect.animationDuration
                animationEnabled: container.animationEnabled && current
                organized: container.organized
                Keys.priority: Keys.AfterItem
                Keys.forwardTo: searchResults
                model: KWinComponents.ClientFilterModel {
                    activity: KWinComponents.Workspace.currentActivity
                    desktop: heap.desktop
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

                    targetScale: {
                        if (!container.anyDesktopBar) return targetScale;
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
                onCurrentChanged: {
                    if (current) {
                        allDesktopHeaps.current = heap;
                    }
                }
                Component.onCompleted: {
                    if (current) {
                        allDesktopHeaps.current = heap;
                    }
                }
            }
        }*/

    }

    Column {
        anchors.left: container.verticalDesktopBar ? desktopBar.right : parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: topBar.bottom

        Item {
            width: parent.width
            height: parent.height - topBar.height

            PlasmaExtras.PlaceholderMessage {
                id: placeholderMessage
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                visible: container.organized && effect.searchText.length > 0 && allDesktopHeaps.current.count === 0
                text: i18nd("kwin_effects", "No matching windows")
            }

            Milou.ResultsView {
                id: searchResults
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: parent.height - placeholderMessage.height - PlasmaCore.Units.largeSpacing
                queryString: effect.searchText
                visible: container.organized && effect.searchText.length > 0 && allDesktopHeaps.current.count === 0

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
            opacity: container.effect.gestureInProgress
                ? 1 - container.effect.partialActivationFactor
                : (model.client.hidden || container.organized) ? 0 : 1

            Behavior on opacity {
                enabled: !container.effect.gestureInProgress
                NumberAnimation { duration: effect.animationDuration; easing.type: Easing.OutCubic }
            }
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
