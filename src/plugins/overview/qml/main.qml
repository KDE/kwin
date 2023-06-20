/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.milou as Milou
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.extras as PlasmaExtras

FocusScope {
    id: container
    focus: true

    readonly property QtObject effect: KWinComponents.SceneView.effect
    readonly property QtObject targetScreen: KWinComponents.SceneView.screen

    readonly property bool lightBackground: Math.max(PlasmaCore.ColorScope.backgroundColor.r,
                                                     PlasmaCore.ColorScope.backgroundColor.g,
                                                     PlasmaCore.ColorScope.backgroundColor.b) > 0.5

    property bool animationEnabled: false
    property bool organized: false

    property alias currentHeap: heapView.currentItem

    function start() {
        animationEnabled = true;
        organized = true;
    }

    function stop() {
        organized = false;
    }

    Keys.onEscapePressed: effect.deactivate();

    Keys.forwardTo: searchField

    Keys.onEnterPressed: {
        currentHeap.forceActiveFocus();
        if (currentHeap.count === 1) {
            currentHeap.activateCurrentClient();
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

    KWinComponents.DesktopBackground {
        id: backgroundItem
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentDesktop
        outputName: targetScreen.name
        property real blurRadius: 0

        layer.enabled: true
        layer.effect: FastBlur {
            radius: backgroundItem.blurRadius
        }
    }

    state: {
        if (effect.gestureInProgress) {
            return "partial";
        } else if (organized) {
            return "active";
        } else {
            return "initial";
        }
    }

    states: [
        State {
            name: "initial"
            PropertyChanges {
                target: underlay
                opacity: 0
            }
            PropertyChanges {
                target: topBar
                opacity: 0
            }
            PropertyChanges {
                target: backgroundItem
                blurRadius: 0
            }
        },
        State {
            name: "partial"
            PropertyChanges {
                target: underlay
                opacity: 0.75 * effect.partialActivationFactor
            }
            PropertyChanges {
                target: topBar
                opacity: effect.partialActivationFactor
            }
            PropertyChanges {
                target: backgroundItem
                blurRadius: 64 * effect.partialActivationFactor
            }
        },
        State {
            name: "active"
            PropertyChanges {
                target: underlay
                opacity: 0.75
            }
            PropertyChanges {
                target: topBar
                opacity: 1
            }
            PropertyChanges {
                target: backgroundItem
                blurRadius: 64
            }
        }
    ]
    transitions: Transition {
        to: "initial, active"
        NumberAnimation {
            duration: effect.animationDuration
            properties: "opacity, blurRadius"
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: underlay
        anchors.fill: parent
        color: PlasmaCore.ColorScope.backgroundColor

        TapHandler {
            onTapped: effect.deactivate();
        }
    }

    Column {
        anchors.fill: parent

        Item {
            id: topBar
            width: parent.width
            height: searchBar.height + desktopBar.height

            Rectangle {
                id: desktopBar
                width: parent.width
                implicitHeight: bar.implicitHeight + 2 * Kirigami.Units.smallSpacing
                color: container.lightBackground ? Qt.rgba(PlasmaCore.ColorScope.backgroundColor.r,
                                                           PlasmaCore.ColorScope.backgroundColor.g,
                                                           PlasmaCore.ColorScope.backgroundColor.b, 0.75)
                                                 : Qt.rgba(0, 0, 0, 0.25)

                DesktopBar {
                    id: bar
                    anchors.fill: parent
                    windowModel: stackModel
                    desktopModel: desktopModel
                    selectedDesktop: KWinComponents.Workspace.currentDesktop
                    heap: currentHeap
                }
            }

            Item {
                id: searchBar
                anchors.top: desktopBar.bottom
                width: parent.width
                height: searchField.height + 2 * Kirigami.Units.gridUnit

                PlasmaExtras.SearchField {
                    id: searchField
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 20 * Kirigami.Units.gridUnit)
                    focus: true
                    Keys.priority: Keys.BeforeItem
                    Keys.forwardTo: text && currentHeap.count === 0 ? searchResults : currentHeap
                    text: effect.searchText
                    onTextEdited: {
                        effect.searchText = text;
                        currentHeap.resetSelected();
                        currentHeap.selectNextItem(WindowHeap.Direction.Down);
                        searchField.focus = true;
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: parent.height - topBar.height

            PlasmaExtras.PlaceholderMessage {
                id: placeholderMessage
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                visible: container.organized && effect.searchText.length > 0 && currentHeap.count === 0
                text: i18nd("kwin", "No matching windows")
            }

            StackView {
                id: heapView
                anchors.fill: parent

                function switchTo(desktop) {
                    container.animationEnabled = false;
                    heapView.replace(heapTemplate, { desktop: desktop });
                    currentItem.layout.forceLayout();
                    container.animationEnabled = true;
                }

                Component.onCompleted: {
                    push(heapTemplate, { desktop: KWinComponents.Workspace.currentDesktop });
                }
            }

            Connections {
                target: KWinComponents.Workspace
                function onCurrentDesktopChanged() {
                    heapView.switchTo(KWinComponents.Workspace.currentDesktop);
                }
            }

            Component {
                id: heapTemplate

                WindowHeap {
                    id: heap

                    required property QtObject desktop

                    visible: !(container.organized && effect.searchText.length > 0) || heap.count !== 0
                    layout.mode: effect.layout
                    focus: true
                    padding: Kirigami.Units.gridUnit
                    animationDuration: effect.animationDuration
                    animationEnabled: container.animationEnabled
                    organized: container.organized
                    Keys.priority: Keys.AfterItem
                    Keys.forwardTo: searchResults
                    model: KWinComponents.WindowFilterModel {
                        activity: KWinComponents.Workspace.currentActivity
                        desktop: heap.desktop
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

                        targetScale: {
                            if (!activeDragHandler.active) {
                                return targetScale; // leave it alone, so it won't affect transitions before they start
                            }
                            var localPressPosition = activeDragHandler.centroid.scenePressPosition.y - heap.layout.Kirigami.ScenePosition.y;
                            if (localPressPosition === 0) {
                                return 0.1;
                            } else {
                                var localPosition = activeDragHandler.centroid.scenePosition.y - heap.layout.Kirigami.ScenePosition.y;
                                return Math.max(0.1, Math.min(localPosition / localPressPosition, 1));
                            }
                        }

                        opacity: 1 - downGestureProgress
                        onDownGestureTriggered: window.closeWindow()

                        TapHandler {
                            acceptedPointerTypes: PointerDevice.GenericPointer | PointerDevice.Pen
                            acceptedButtons: Qt.MiddleButton
                            onTapped: window.closeWindow()
                        }
                    }
                    onActivated: effect.deactivate();
                }
            }

            Milou.ResultsView {
                id: searchResults
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: parent.height - placeholderMessage.height - Kirigami.Units.gridUnit
                queryString: effect.searchText
                visible: container.organized && effect.searchText.length > 0 && currentHeap.count === 0

                onActivated: {
                    effect.deactivate();
                }
            }
        }
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
            opacity: container.effect.gestureInProgress
                ? 1 - container.effect.partialActivationFactor
                : (model.window.hidden || container.organized) ? 0 : 1

            Behavior on opacity {
                enabled: !container.effect.gestureInProgress
                NumberAnimation { duration: effect.animationDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    KWinComponents.WindowModel {
        id: stackModel
    }

    KWinComponents.VirtualDesktopModel {
        id: desktopModel
    }

    Component.onCompleted: start();
}
