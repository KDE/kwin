/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Window 2.12
import org.kde.kwin 2.0 as KWinComponents
import org.kde.kwin.scripts.expo 1.0
import org.kde.plasma.core 2.0 as PlasmaCore

Window {
    id: container

    readonly property int animationDuration: PlasmaCore.Units.longDuration

    readonly property bool skipOpenAnimation: true
    readonly property bool skipCloseAnimation: true

    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    x: Screen.virtualX
    y: Screen.virtualY
    width: Screen.width
    height: Screen.height
    color: "transparent"
    visible: true

    function stop() {
        for (let i = 0; i < windowsRepeater.count; ++i) {
            windowsRepeater.itemAt(i).state = "initial";
        }
    }

    ExpoLayout {
        id: expoLayout
        anchors.fill: parent
        mode: ExpoLayout.LayoutNatural
    }

    KWinComponents.ThumbnailItem {
        id: desktopBackground
        anchors.fill: parent
    }

    Repeater {
        id: windowsRepeater
        model: KWinComponents.ClientModel {
            exclusions: KWinComponents.ClientModel.DesktopWindowsExclusion |
                    KWinComponents.ClientModel.DockWindowsExclusion;
        }

        KWinComponents.ThumbnailItem {
            id: windowThumbnail
            wId: model.client.internalId
            state: "initial"

            ExpoCell {
                id: cell
                layout: expoLayout
                naturalX: model.client.x - Screen.virtualX
                naturalY: model.client.y - Screen.virtualY
                naturalWidth: model.client.width
                naturalHeight: model.client.height
            }

            states: [
                State {
                    name: "initial"
                    PropertyChanges {
                        target: windowThumbnail
                        x: model.client.x - Screen.virtualX
                        y: model.client.y - Screen.virtualY
                        width: model.client.width
                        height: model.client.height
                    }
                },
                State {
                    name: "active"
                    PropertyChanges {
                        target: windowThumbnail
                        x: cell.x
                        y: cell.y
                        width: cell.width
                        height: cell.height
                    }
                }
            ]

            Behavior on x {
                NumberAnimation { duration: container.animationDuration; easing.type: Easing.InOutCubic; }
            }
            Behavior on y {
                NumberAnimation { duration: container.animationDuration; easing.type: Easing.InOutCubic; }
            }
            Behavior on width {
                NumberAnimation { duration: container.animationDuration; easing.type: Easing.InOutCubic; }
            }
            Behavior on height {
                NumberAnimation { duration: container.animationDuration; easing.type: Easing.InOutCubic; }
            }

            Component.onCompleted: windowThumbnail.state = "active"
        }
    }

    Component.onCompleted: {
        const clients = workspace.clientList();
        for (let i = 0; i < clients.length; ++i) {
            if (clients[i].desktopWindow && clients[i].screen == index) {
                desktopBackground.wId = clients[i].internalId;
            }
        }
        KWin.registerWindow(container);
    }
}
