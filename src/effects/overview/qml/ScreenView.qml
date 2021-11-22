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
    }

    function stop() {
        container.organized = false;
    }

    Keys.onEscapePressed: effect.deactivate();

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: searchField

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

    ExpoArea {
        id: heapArea
        screen: targetScreen
    }

    Column {
        x: heapArea.x
        y: heapArea.y
        width: heapArea.width
        height: heapArea.height

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
                    Keys.priority: Keys.AfterItem
                    Keys.forwardTo: text ? searchResults : heap
                    onTextEdited: forceActiveFocus();
                }
            }
        }

        Item {
            width: parent.width
            height: parent.height - topBar.height

            WindowHeap {
                id: heap
                visible: !(container.organized && searchField.text)
                anchors.fill: parent
                layout: effect.layout
                padding: PlasmaCore.Units.largeSpacing
                animationDuration: effect.animationDuration
                animationEnabled: container.animationEnabled
                organized: container.organized
                model: KWinComponents.ClientFilterModel {
                    activity: KWinComponents.Workspace.currentActivity
                    desktop: KWinComponents.Workspace.currentVirtualDesktop
                    screenName: targetScreen.name
                    clientModel: stackModel
                    windowType: ~KWinComponents.ClientFilterModel.Dock &
                            ~KWinComponents.ClientFilterModel.Desktop &
                            ~KWinComponents.ClientFilterModel.Notification;
                }
                onActivated: effect.deactivate();
            }

            Milou.ResultsView {
                id: searchResults
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: Math.min(contentHeight, parent.height)
                queryString: searchField.text
                visible: container.organized && searchField.text

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
            visible: !model.client.hidden
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            width: model.client.width
            height: model.client.height

            TapHandler {
                onTapped: effect.deactivate();
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
