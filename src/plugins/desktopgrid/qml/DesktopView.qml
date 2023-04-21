/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components 3.0 as PC3
import org.kde.kwin.private.desktopgrid

FocusScope {
    id: desktopView

    required property QtObject windowModel
    required property QtObject desktop
    required property var dndManagerStore
    readonly property bool dragActive: heap.dragActive || dragHandler.active || xAnim.running || yAnim.running
    property real panelOpacity: 1
    focus: true

    function selectLastItem(direction) {
        heap.selectLastItem(direction);
    }

    DropArea {
        anchors.fill: parent
        onEntered: {
            drag.accepted = true;
        }
        onDropped: drop => {
            drop.accepted = true;
            if (drag.source instanceof DesktopView) {
                // dragging a desktop as a whole
                if (drag.source === desktopView) {
                    drop.action = Qt.IgnoreAction;
                    return;
                }
                effect.swapDesktops(drag.source.desktop.x11DesktopNumber, desktop.x11DesktopNumber);
            } else {
                // dragging a KWin::Window
                if (drag.source.desktops.length === 0 || drag.source.desktops.indexOf(desktopView.desktop) !== -1) {
                    drop.action = Qt.IgnoreAction;
                    return;
                }
                drag.source.desktops = [desktopView.desktop];
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
            if (!desktopView.contains(desktopView.mapFromItem(null, pos.x, pos.y))) {
                return;
            }
            item.client.desktops = [desktopView.desktop];
        }
    }
    Repeater {
        model: KWinComponents.WindowFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            screenName: targetScreen.name
            windowModel: desktopView.windowModel
            windowType: KWinComponents.WindowFilterModel.Dock | KWinComponents.WindowFilterModel.Desktop
        }

        KWinComponents.WindowThumbnail {
            wId: model.window.internalId
            x: model.window.x - targetScreen.geometry.x
            y: model.window.y - targetScreen.geometry.y
            z: model.window.stackingOrder
            width: model.window.width
            height: model.window.height
            opacity: model.window.dock ? desktopView.panelOpacity : 1
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

    WindowHeap {
        id: heap
        function resetPosition() {
            x = 0;
            y = 0;
        }
        Drag.active: dragHandler.active
        Drag.proposedAction: Qt.MoveAction
        Drag.supportedActions: Qt.MoveAction
        Drag.source: desktopView
        Drag.hotSpot: Qt.point(width * 0.5, height * 0.5)
        width: parent.width
        height: parent.height
        focus: true
        z: 9999
        animationDuration: container.effect.animationDuration
        absolutePositioning: false
        animationEnabled: container.animationEnabled
        organized: container.organized
        layout.mode: effect.layout
        dndManagerStore: desktopView.dndManagerStore
        model: KWinComponents.WindowFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            screenName: targetScreen.name
            windowModel: desktopView.windowModel
            windowType: ~KWinComponents.WindowFilterModel.Dock &
                        ~KWinComponents.WindowFilterModel.Desktop &
                        ~KWinComponents.WindowFilterModel.Notification &
                        ~KWinComponents.WindowFilterModel.CriticalNotification
        }
        delegate: WindowHeapDelegate {
            windowHeap: heap
            closeButtonVisible: false
            windowTitleVisible: false

            TapHandler {
                acceptedPointerTypes: PointerDevice.GenericPointer | PointerDevice.Pen
                acceptedButtons: Qt.MiddleButton | Qt.RightButton
                onTapped: (eventPoint, button) => {
                    if (button === Qt.MiddleButton) {
                        window.closeWindow();
                    } else if (button === Qt.RightButton) {
                        if (window.desktops.length > 0) {
                            window.desktops = [];
                        } else {
                            window.desktops = [desktopView.desktop];
                        }
                    }
                }
            }
        }
        onActivated: effect.deactivate(effect.animationDuration);
        Behavior on x {
            enabled: !dragHandler.active
            NumberAnimation {
                id: xAnim
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on y {
            enabled: !dragHandler.active
            NumberAnimation {
                id: yAnim
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        anchors.fill: heap
        acceptedButtons: Qt.NoButton
        cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
    }

    PC3.Control {
        id: desktopLabel
        anchors.margins: PlasmaCore.Units.largeSpacing
        z: 9999
        visible: effect.desktopNameAlignment !== 0

        states: [
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignTop | Qt.AlignLeft)
                AnchorChanges {
                    target: desktopLabel
                    anchors.left: desktopView.left
                    anchors.top: desktopView.top
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.TopLeft
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignTop | Qt.AlignHCenter)
                AnchorChanges {
                    target: desktopLabel
                    anchors.top: desktopView.top
                    anchors.horizontalCenter: desktopView.horizontalCenter
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.Top
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignTop | Qt.AlignRight)
                AnchorChanges {
                    target: desktopLabel
                    anchors.top: desktopView.top
                    anchors.right: desktopView.right
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.TopRight
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignVCenter | Qt.AlignLeft)
                AnchorChanges {
                    target: desktopLabel
                    anchors.left: desktopView.left
                    anchors.verticalCenter: desktopView.verticalCenter
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.Left
                }
            },
            State {
                when: container.effect.desktopNameAlignment === Qt.AlignCenter
                AnchorChanges {
                    target: desktopLabel
                    anchors.horizontalCenter: desktopView.horizontalCenter
                    anchors.verticalCenter: desktopView.verticalCenter
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.Center
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignVCenter | Qt.AlignRight)
                AnchorChanges {
                    target: desktopLabel
                    anchors.verticalCenter: desktopView.verticalCenter
                    anchors.right: desktopView.right
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.Right
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignBottom | Qt.AlignLeft)
                AnchorChanges {
                    target: desktopLabel
                    anchors.left: desktopView.left
                    anchors.bottom: desktopView.bottom
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.BottomLeft
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignBottom | Qt.AlignHCenter)
                AnchorChanges {
                    target: desktopLabel
                    anchors.bottom: desktopView.bottom
                    anchors.horizontalCenter: desktopView.horizontalCenter
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.Bottom
                }
            },
            State {
                when: container.effect.desktopNameAlignment === (Qt.AlignBottom | Qt.AlignRight)
                AnchorChanges {
                    target: desktopLabel
                    anchors.bottom: desktopView.bottom
                    anchors.right: desktopView.right
                }
                PropertyChanges {
                    target: desktopLabel
                    transformOrigin: Item.BottomRight
                }
            }
        ]

        scale: 1 / desktopView.parent.scale
        leftPadding: PlasmaCore.Units.smallSpacing
        rightPadding: PlasmaCore.Units.smallSpacing
        contentItem: PC3.Label {
            text: desktopView.desktop.name
        }
        background: Rectangle {
            color: PlasmaCore.Theme.backgroundColor
            radius: height
            opacity: 0.6
        }
    }
}
