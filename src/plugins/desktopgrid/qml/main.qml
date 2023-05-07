/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components 3.0 as PC3

Rectangle {
    id: container

    readonly property QtObject effect: KWinComponents.SceneView.effect
    readonly property QtObject targetScreen: KWinComponents.SceneView.screen

    property bool animationEnabled: false
    property bool organized: false

    /** Shared Drag&Drop store to keep track of DND state across desktops. */
    property var dndManagerStore: ({})

    color: "black"

    function start() {
        animationEnabled = true;
        organized = true;
    }

    function stop() {
        organized = false;
    }

    function switchTo(desktop) {
        KWinComponents.Workspace.currentDesktop = desktop;
        effect.deactivate();
    }

    function selectNext(direction) {
        let currentIndex = 0
        for (let i = 0; i < gridRepeater.count; i++) {
            if (gridRepeater.itemAt(i).focus) {
                currentIndex = i;
                break;
            }
        }
        let x = currentIndex % grid.columns;
        let y = Math.floor(currentIndex / grid.columns);

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

        if (x < 0 || x >= grid.columns) {
            return false;
        }
        if (y < 0 || y >= grid.rows) {
            return false;
        }
        let newIndex = y * grid.columns + x;

        gridRepeater.itemAt(newIndex).focus = true;
        gridRepeater.itemAt(newIndex).selectLastItem(invertedDirection);
        return true;
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Escape) {
            effect.deactivate();
        } else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            addButton.clicked();
        } else if (event.key === Qt.Key_Minus) {
            removeButton.clicked();
        } else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) {
            const desktopId = event.key - Qt.Key_F1;
            if (desktopId < gridRepeater.count) {
                switchTo(gridRepeater.itemAt(desktopId).desktop);
            }
        } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            const desktopId = event.key === Qt.Key_0 ? 9 : (event.key - Qt.Key_1);
            if (desktopId < gridRepeater.count) {
                switchTo(gridRepeater.itemAt(desktopId).desktop);
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
            for (let i = 0; i < gridRepeater.count; i++) {
                if (gridRepeater.itemAt(i).focus) {
                    switchTo(gridRepeater.itemAt(i).desktop)
                    break;
                }
            }
        }
    }
    Keys.priority: Keys.AfterItem

    KWinComponents.VirtualDesktopModel {
        id: desktopModel
    }
    KWinComponents.WindowModel {
        id: stackModel
    }

    // A grid, not a gridlayout as a gridlayout positions its elements too late
    Grid {
        id: grid

        property Item currentItem
        readonly property real targetScale: Math.min(parent.width / width, parent.height / height)
        property real panelOpacity: 1

        Behavior on x {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on y {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on scale {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on panelOpacity {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        width: (parent.width + columnSpacing) * columns - columnSpacing
        height: (parent.height + rowSpacing) * rows - rowSpacing
        rowSpacing: PlasmaCore.Units.gridUnit
        columnSpacing: PlasmaCore.Units.gridUnit
        rows: container.effect.gridRows
        columns: container.effect.gridColumns
        transformOrigin: Item.TopLeft

        states: [
            State {
                when: container.effect.gestureInProgress
                PropertyChanges {
                    target: grid
                    x: Math.max(0, container.width / 2 - (grid.width * grid.targetScale) / 2) * container.effect.partialActivationFactor - grid.currentItem.x * (1 - container.effect.partialActivationFactor)
                    y: Math.max(0, container.height / 2 - (grid.height * grid.targetScale) / 2) * container.effect.partialActivationFactor - grid.currentItem.y * (1 - container.effect.partialActivationFactor)
                    scale: 1 - (1 - grid.targetScale) * container.effect.partialActivationFactor
                    panelOpacity: 1 - container.effect.partialActivationFactor
                }
                PropertyChanges {
                    target: buttonsLayout
                    opacity: container.effect.partialActivationFactor
                }
            },
            State {
                when: container.organized
                PropertyChanges {
                    target: grid
                    x: Math.max(0, container.width / 2 - (grid.width * grid.targetScale) / 2)
                    y: Math.max(0, container.height / 2 - (grid.height * grid.targetScale) / 2)
                    scale: grid.targetScale
                    panelOpacity: 0
                }
                PropertyChanges {
                    target: buttonsLayout
                    opacity: 1
                }
            },
            State {
                when: !container.organized
                PropertyChanges {
                    target: grid
                    x: -grid.currentItem.x
                    y: -grid.currentItem.y
                    scale: 1
                    panelOpacity: 1
                }
                PropertyChanges {
                    target: buttonsLayout
                    opacity: 0
                }
            }
        ]
        Repeater {
            id: gridRepeater
            model: desktopModel
            DesktopView {
                id: thumbnail

                panelOpacity: grid.panelOpacity
                readonly property bool current: KWinComponents.Workspace.currentDesktop === desktop
                z: dragActive ? 1 : 0
                onCurrentChanged: {
                    if (current) {
                        grid.currentItem = thumbnail;
                    }
                }
                Component.onCompleted: {
                    if (current) {
                        grid.currentItem = thumbnail;
                    }
                }
                width: container.width
                height: container.height

                windowModel: stackModel
                dndManagerStore: container.dndManagerStore
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border {
                        color: PlasmaCore.Theme.highlightColor
                        width: 1 / grid.scale
                    }
                    visible: parent.activeFocus
                }
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        KWinComponents.Workspace.currentDesktop = thumbnail.desktop;
                        container.effect.deactivate();
                    }
                }
            }
        }
    }

    RowLayout {
        id: buttonsLayout
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: PlasmaCore.Units.smallSpacing
        }
        spacing: PlasmaCore.Units.smallSpacing
        visible: container.effect.showAddRemove
        PC3.Button {
            id: addButton
            icon.name: "list-add"
            onClicked: container.effect.addDesktop()
        }
        PC3.Button {
            id: removeButton
            icon.name: "list-remove"
            onClicked: container.effect.removeDesktop()
        }
        Behavior on opacity {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            container.effect.deactivate();
        }
    }

    Component.onCompleted: start()
}
