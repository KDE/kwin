/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.extras as PlasmaExtras
import org.kde.KWin.Effect.WindowView

Item {
    id: container

    readonly property QtObject effect: KWinComponents.SceneView.effect
    readonly property QtObject targetScreen: KWinComponents.SceneView.screen

    property bool animationEnabled: false
    property bool organized: false

    function start() {
        animationEnabled = true;
        organized = true;
    }

    function stop() {
        organized = false;
    }

    Keys.onEscapePressed: effect.windowPicked(null);

    Keys.priority: Keys.AfterItem
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
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentDesktop
        outputName: targetScreen.name

        layer.enabled: true
        layer.effect: FastBlur {
            radius: container.organized ? 64 : 0
            Behavior on radius {
                NumberAnimation { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
        opacity: container.organized ? 0.75 : 0

        // TapHandler {
        //     onTapped: effect.deactivate(container.effect.animationDuration);
        // }

        Behavior on opacity {
            OpacityAnimator { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
        }
    }

    Loader {
        anchors.centerIn: parent
        width: parent.width - (Kirigami.Units.gridUnit * 8)

        active: heap.activeEmpty

        // Otherwise it's always 100% opaque even while the blurry desktop background's
        // opacity is changing, which looks weird and is different from what Overview does.
        opacity: container.organized ? 1 : 0
        Behavior on opacity {
            OpacityAnimator { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
        }

        sourceComponent: PlasmaExtras.PlaceholderMessage {
            iconName: "edit-none"
            text: i18nd("kwin", "No Windows")
        }
    }

    ColumnLayout {
        width: targetScreen.geometry.width
        height: targetScreen.geometry.height

        PlasmaExtras.Heading {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit
            level: 1
            type: PlasmaExtras.Heading.Primary
            text: i18nd("kwin", "Select Window to Screenshot")
        }

        WindowHeap {
            id: heap
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            padding: Kirigami.Units.gridUnit
            animationDuration: container.effect.animationDuration
            animationEnabled: container.animationEnabled
            organized: container.organized
            activateWindow: false
            model: KWinComponents.WindowFilterModel {
                activity: KWinComponents.Workspace.currentActivity
                desktop: KWinComponents.Workspace.currentDesktop;
                screenName: targetScreen.name
                windowModel: stackModel
                minimizedWindows: !effect.ignoreMinimized
                windowType: ~KWinComponents.WindowFilterModel.Dock &
                            ~KWinComponents.WindowFilterModel.Desktop &
                            ~KWinComponents.WindowFilterModel.Notification &
                            ~KWinComponents.WindowFilterModel.CriticalNotification
            }
            delegate: WindowHeapDelegate {
                id: delegate
                windowHeap: heap
                dragEnabled: false
                closeButtonVisible: false
                live: container.effect.livePreviews
                partialActivationFactor: container.organized ? 1 : 0
                contentItemParent: container
                Behavior on partialActivationFactor {
                    SequentialAnimation {
                        PropertyAction {
                            target: delegate
                            property: "gestureInProgress"
                            value: true
                        }
                        NumberAnimation {
                            duration: container.effect.animationDuration
                            easing.type: Easing.OutCubic
                        }
                        PropertyAction {
                            target: delegate
                            property: "gestureInProgress"
                            value: false
                        }
                    }
                }
            }
            onActivated: container.effect.windowPicked(heap.model.index(heap.selectedIndex, 0).data());
        }
    }

    Instantiator {
        asynchronous: true

        model: KWinComponents.WindowFilterModel {
            desktop: KWinComponents.Workspace.currentDesktop
            screenName: targetScreen.name
            windowModel: stackModel
            windowType: KWinComponents.WindowFilterModel.Dock
        }

        KWinComponents.WindowThumbnail {
            id: windowThumbnail
            wId: model.window.internalId
            x: model.window.x - targetScreen.geometry.x
            y: model.window.y - targetScreen.geometry.y
            z: model.window.stackingOrder
            visible: opacity > 0
            opacity: (model.window.hidden || container.organized) ? 0 : 1

            Behavior on opacity {
                NumberAnimation { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
            }
        }

        onObjectAdded: (index, object) => {
            object.parent = container
        }
    }

    KWinComponents.WindowModel {
        id: stackModel
    }

    Component.onCompleted: start();
}
