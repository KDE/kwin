/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PC3

Rectangle {
    id: container

    required property QtObject effect
    required property QtObject targetScreen

    property bool animationEnabled: false
    property bool organized: false

    color: "black"

    function start() {
        container.animationEnabled = true;
        container.organized = true;
    }

    function stop() {
        container.organized = false;
    }

    function switchTo(desktopId) {
        KWinComponents.Workspace.currentDesktop = desktopId;
        container.effect.deactivate(container.effect.animationDuration);
    }

    Keys.onPressed: {
        if (event.key == Qt.Key_Escape) {
            effect.deactivate(effect.animationDuration);
        } else if (event.key == Qt.Key_Plus || event.key == Qt.Key_Equal) {
            addButton.clicked();
        } else if (event.key == Qt.Key_Minus) {
            removeButton.clicked();
        } else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) {
            const desktopId = (event.key - Qt.Key_F1) + 1;
            switchTo(desktopId);
        } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            const desktopId = event.key == Qt.Key_0 ? 10 : (event.key - Qt.Key_0);
            switchTo(desktopId);
        }
    }
    Keys.priority: Keys.AfterItem

    KWinComponents.VirtualDesktopModel {
        id: desktopModel
    }
    KWinComponents.ClientModel {
        id: stackModel
    }

    // A grid, not a gridlayout as a gridlayout positions its elements too late
    Grid {
        id: grid

        property Item currentItem
        readonly property real targetScale : 1 / Math.max(rows, columns)
        property real panelOpacity

        Behavior on x {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.InOutCubic
            }
        }
        Behavior on y {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.InOutCubic
            }
        }
        Behavior on scale {
            enabled: !container.effect.gestureInProgress
            NumberAnimation {
                duration: container.effect.animationDuration
                easing.type: Easing.InOutCubic
            }
        }

        width: parent.width * columns
        height: parent.height * rows
        rowSpacing: PlasmaCore.Units.largeSpacing
        columnSpacing: PlasmaCore.Units.largeSpacing
        rows: container.effect.gridRows
        columns: container.effect.gridColumns
        transformOrigin: Item.TopLeft

        states: [
            State {
                when: container.effect.gestureInProgress
                PropertyChanges {
                    target: grid
                    x: Math.max(0, container.width / 2 - (width * targetScale) / 2) * container.effect.partialActivationFactor - grid.currentItem.x * (1 - container.effect.partialActivationFactor)
                    y: Math.max(0, container.height / 2 - (height * targetScale) / 2) * container.effect.partialActivationFactor - grid.currentItem.y * (1 - container.effect.partialActivationFactor)
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
                    x: Math.max(0, container.width / 2 - (width * targetScale) / 2)
                    y: Math.max(0, container.height / 2 - (height * targetScale) / 2)
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
            model: desktopModel
            DesktopView {
                id: thumbnail

                panelOpacity: grid.panelOpacity
                readonly property bool current: KWinComponents.Workspace.currentVirtualDesktop === desktop
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

                clientModel: stackModel
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        KWinComponents.Workspace.currentVirtualDesktop = thumbnail.desktop;
                        container.effect.deactivate(container.effect.animationDuration);
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
                easing.type: Easing.InOutCubic
            }
        }
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            container.effect.deactivate(container.effect.animationDuration);
        }
    }

    Component.onCompleted: start()
}
