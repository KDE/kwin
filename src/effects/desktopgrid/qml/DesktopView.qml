/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PC3
import org.kde.kwin.private.desktopgrid 1.0

FocusScope {
    id: desktopView

    required property QtObject clientModel
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
                if (drag.source.desktop === desktopView.desktop.x11DesktopNumber) {
                    drop.action = Qt.IgnoreAction;
                    return;
                }
                drag.source.desktop = desktopView.desktop.x11DesktopNumber;
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
            item.client.desktop = desktopView.desktop.x11DesktopNumber;
        }
    }
    Repeater {
        model: KWinComponents.ClientFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            screenName: targetScreen.name
            clientModel: desktopView.clientModel
            windowType: KWinComponents.ClientFilterModel.Dock | KWinComponents.ClientFilterModel.Desktop
        }

        KWinComponents.WindowThumbnailItem {
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            z: model.client.stackingOrder
            width: model.client.width
            height: model.client.height
            opacity: model.client.dock ? desktopView.panelOpacity : 1
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
        model: KWinComponents.ClientFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            screenName: targetScreen.name
            clientModel: desktopView.clientModel
            windowType: ~KWinComponents.ClientFilterModel.Dock &
                        ~KWinComponents.ClientFilterModel.Desktop &
                        ~KWinComponents.ClientFilterModel.Notification &
                        ~KWinComponents.ClientFilterModel.CriticalNotification
        }
        delegate: WindowHeapDelegate {
            windowHeap: heap
            closeButtonVisible: false
            windowTitleVisible: false
        }
        onActivated: effect.deactivate(effect.animationDuration);
        onWindowClicked: {
            if (eventPoint.event.button === Qt.MiddleButton) {
                window.closeWindow();
            } else if (eventPoint.event.button === Qt.RightButton) {
                if (window.desktop > -1) {
                    window.desktop = -1;
                } else {
                    window.desktop = desktopView.desktop.x11DesktopNumber;
                }
            }
        }
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
