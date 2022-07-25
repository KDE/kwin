/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtGraphicalEffects 1.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.milou 0.3 as Milou
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.kirigami 2.12 as Kirigami

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

    function start() {
        container.animationEnabled = true;
        container.organized = true;
        searchField.text = "";
    }

    function stop() {
        container.organized = false;
    }

    Keys.onEscapePressed: effect.deactivate();

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: searchField
    Keys.onEnterPressed: {
        heap.forceActiveFocus();
        if (heap.count === 1) {
            heap.activateCurrentClient();
        }
    }

    KWinComponents.DesktopBackgroundItem {
        id: backgroundItem
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentVirtualDesktop
        outputName: targetScreen.name
        property real blurRadius: 0

        layer.enabled: effect.blurBackground
        layer.effect: FastBlur {
            radius: backgroundItem.blurRadius
        }
    }

    state: {
        if (effect.gestureInProgress) {
            return "partial";
        } else if (container.organized) {
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
                implicitHeight: bar.implicitHeight + 2 * PlasmaCore.Units.smallSpacing
                color: container.lightBackground ? Qt.rgba(PlasmaCore.ColorScope.backgroundColor.r,
                                                           PlasmaCore.ColorScope.backgroundColor.g,
                                                           PlasmaCore.ColorScope.backgroundColor.b, 0.75)
                                                 : Qt.rgba(0, 0, 0, 0.25)

                DesktopBar {
                    id: bar
                    anchors.fill: parent
                    clientModel: stackModel
                    desktopModel: desktopModel
                    selectedDesktop: KWinComponents.Workspace.currentVirtualDesktop
                }
            }

            Item {
                id: searchBar
                anchors.top: desktopBar.bottom
                width: parent.width
                height: searchField.height + 2 * PlasmaCore.Units.largeSpacing

                PC3.TextField {
                    id: searchField
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 20 * PlasmaCore.Units.gridUnit)
                    focus: true
                    placeholderText: i18nd("kwin_effects", "Search...")
                    clearButtonShown: true
                    Keys.priority: Keys.BeforeItem
                    Keys.forwardTo: text && heap.count === 0 ? searchResults : heap
                    onTextChanged: {
                        effect.searchText = text;
                        heap.resetSelected();
                        heap.selectNextItem(WindowHeap.Direction.Down);
                    }
                     Binding {
                        target: searchField
                        property: "text"
                        value: effect.searchText
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: parent.height - topBar.height

            PlasmaExtras.PlaceholderMessage {
                id: placeholderMessage
                anchors.top: parent
                anchors.horizontalCenter: parent.horizontalCenter
                visible: container.organized && searchField.text && heap.count === 0
                text: i18nd("kwin_effects", "No matching windows")
            }

            WindowHeap {
                id: heap
                visible: !(container.organized && searchField.text) || heap.count !== 0
                anchors.fill: parent
                layout.mode: effect.layout
                focus: true
                padding: PlasmaCore.Units.largeSpacing
                animationDuration: effect.animationDuration
                animationEnabled: container.animationEnabled
                organized: container.organized
                onWindowClicked: {
                    if (eventPoint.event.button !== Qt.MiddleButton) {
                        return;
                    }
                    window.closeWindow();
                }
                Keys.priority: Keys.AfterItem
                Keys.forwardTo: searchResults
                model: KWinComponents.ClientFilterModel {
                    activity: KWinComponents.Workspace.currentActivity
                    desktop: KWinComponents.Workspace.currentVirtualDesktop
                    screenName: targetScreen.name
                    clientModel: stackModel
                    filter: searchField.text
                    minimizedWindows: !effect.ignoreMinimized
                    windowType: ~KWinComponents.ClientFilterModel.Dock &
                            ~KWinComponents.ClientFilterModel.Desktop &
                            ~KWinComponents.ClientFilterModel.Notification;
                }
                onActivated: effect.deactivate();
                delegate: WindowHeapDelegate {
                    windowHeap: heap

                    targetScale: {
                        var localPressPosition = dragHandler.centroid.scenePressPosition.y - heap.layout.Kirigami.ScenePosition.y;
                        if (localPressPosition == 0) {
                            return 0.1
                        } else {
                            var localPosition = dragHandler.centroid.scenePosition.y - heap.layout.Kirigami.ScenePosition.y;
                            return Math.max(0.1, Math.min(localPosition / localPressPosition, 1))
                        }
                    }

                    opacity: 1 - downGestureProgress
                    onDownGestureTriggered: client.closeWindow()
                }
            }

            Milou.ResultsView {
                id: searchResults
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: parent.height - placeholderMessage.height - PlasmaCore.Units.largeSpacing
                queryString: searchField.text
                visible: container.organized && searchField.text && heap.count === 0

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

    Component.onCompleted: start();
}
