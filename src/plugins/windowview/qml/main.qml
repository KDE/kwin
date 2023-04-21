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
import org.kde.plasma.core as PlasmaCore
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

    Keys.onEscapePressed: effect.deactivate(container.effect.animationDuration);

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: searchField
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
        color: PlasmaCore.ColorScope.backgroundColor
        opacity: container.organized ? 0.75 : 0

        TapHandler {
            onTapped: effect.deactivate(container.effect.animationDuration);
        }

        Behavior on opacity {
            OpacityAnimator { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
        }
    }

    Loader {
        anchors.centerIn: parent
        width: parent.width - (PlasmaCore.Units.gridUnit * 8)

        active: heap.activeEmpty

        // Otherwise it's always 100% opaque even while the blurry desktop background's
        // opacity is changing, which looks weird and is different from what Overview does.
        opacity: container.organized ? 1 : 0
        Behavior on opacity {
            OpacityAnimator { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
        }

        sourceComponent: PlasmaExtras.PlaceholderMessage {
            iconName: "edit-none"
            text: effect.searchText.length > 0 ? i18nd("kwin", "No Matches") : i18nd("kwin", "No Windows")
        }
    }

    ColumnLayout {
        width: targetScreen.geometry.width
        height: targetScreen.geometry.height
        PlasmaExtras.SearchField {
            id: searchField
            Layout.alignment: Qt.AlignCenter
            Layout.topMargin: PlasmaCore.Units.gridUnit
            Layout.preferredWidth: Math.min(parent.width, 20 * PlasmaCore.Units.gridUnit)
            focus: false

            // Don't confuse users into thinking it's a full search
            placeholderText: i18nd("kwin", "Filter windows…")

            // Otherwise it's always 100% opaque even while the blurry desktop background's
            // opacity is changing, which looks weird and is different from what Overview does.
            opacity: container.organized ? 1 : 0
            Behavior on opacity {
                OpacityAnimator { duration: container.effect.animationDuration; easing.type: Easing.OutCubic }
            }

            // We can't use activeFocus because is not reliable on qml effects
            text: effect.searchText
            onTextEdited: {
                effect.searchText = text;
                heap.resetSelected();
                heap.selectNextItem(WindowHeap.Direction.Down);
            }
            Keys.priority: Keys.AfterItem
            Keys.forwardTo: heap
            Keys.onPressed: {
                switch (event.key) {
                case Qt.Key_Down:
                    heap.forceActiveFocus();
                    break;
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    heap.forceActiveFocus();
                    if (heap.count === 1) {
                        heap.activateIndex(0);
                    }
                    break;
                default:
                    break;
                }
            }
            Keys.onEnterPressed: {
                heap.forceActiveFocus();
                if (heap.count === 1) {
                    heap.activateCurrentClient();
                }
            }
        }
        WindowHeap {
            id: heap
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            padding: PlasmaCore.Units.largeSpacing
            animationDuration: container.effect.animationDuration
            animationEnabled: container.animationEnabled
            organized: container.organized
            showOnly: {
                switch (container.effect.mode) {
                    case WindowView.ModeWindowClass:
                    case WindowView.ModeWindowClassCurrentDesktop:
                        return "activeClass";
                    default:
                        return container.effect.selectedIds;
                }
            }
            layout.mode: effect.layout
            model: KWinComponents.WindowFilterModel {
                activity: KWinComponents.Workspace.currentActivity
                desktop: {
                    switch (container.effect.mode) {
                        case WindowView.ModeCurrentDesktop:
                        case WindowView.ModeWindowClassCurrentDesktop:
                            return KWinComponents.Workspace.currentDesktop;
                        default:
                            return undefined;
                    }
                }
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
                opacity: 1 - downGestureProgress
                onDownGestureTriggered: window.closeWindow()

                TapHandler {
                    acceptedPointerTypes: PointerDevice.GenericPointer | PointerDevice.Pen
                    acceptedButtons: Qt.MiddleButton
                    onTapped: window.closeWindow();
                }
            }
            onActivated: effect.deactivate(container.effect.animationDuration);
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
