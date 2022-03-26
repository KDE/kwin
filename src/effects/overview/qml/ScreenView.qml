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
import org.kde.kirigami 2.12 as Kirigami

FocusScope {
    id: container
    focus: true

    required property QtObject effect
    required property QtObject targetScreen

    readonly property bool lightBackground: Math.max(PlasmaCore.ColorScope.backgroundColor.r,
                                                     PlasmaCore.ColorScope.backgroundColor.g,
                                                     PlasmaCore.ColorScope.backgroundColor.b) > 0.5

    property bool organized: false

    function start() {
        container.organized = true;
    }

    function stop() {
        container.organized = false;
    }

    Keys.onEscapePressed: effect.ungrabActive();

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: searchField

    KWinComponents.DesktopBackgroundItem {
        id: backgroundItem
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentVirtualDesktop
        outputName: targetScreen.name

        layer.enabled: effect.blurBackground
        layer.effect: FastBlur {
            radius: 64 * Math.constrain(effect.partialActivationFactor, 0.0, 1.0)
        }
    }

    Rectangle {
        id: underlay
        anchors.fill: parent
        color: PlasmaCore.ColorScope.backgroundColor
        opacity: 0.75 * effect.partialActivationFactor

        TapHandler {
            onTapped: effect.ungrabActive();
        }
    }

    Column {
        anchors.fill: parent

        Item {
            id: topBar
            width: parent.width
            height: searchBar.height + desktopBar.height
            opacity: effect.partialActivationFactor

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
                opacity: effect.partialActivationFactor

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
                layout.mode: effect.layout
                padding: PlasmaCore.Units.largeSpacing
                animationDuration: 200
                animationEnabled: true
                organized: container.organized
                onWindowClicked: {
                    if (eventPoint.event.button !== Qt.MiddleButton) {
                        return;
                    }
                    window.closeWindow();
                }
                model: KWinComponents.ClientFilterModel {
                    activity: KWinComponents.Workspace.currentActivity
                    desktop: KWinComponents.Workspace.currentVirtualDesktop
                    screenName: targetScreen.name
                    clientModel: stackModel
                    minimizedWindows: !effect.ignoreMinimized
                    windowType: ~KWinComponents.ClientFilterModel.Dock &
                            ~KWinComponents.ClientFilterModel.Desktop &
                            ~KWinComponents.ClientFilterModel.Notification;
                }
                onActivated: effect.ungrabActive();
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
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width / 2
                height: Math.min(contentHeight, parent.height)
                queryString: searchField.text
                visible: container.organized && searchField.text

                onActivated: {
                    effect.ungrabActive();
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
            opacity: container.effect.partialActivationFactor

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
