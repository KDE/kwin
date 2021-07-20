/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.plasma.core 2.0 as PlasmaCore

FocusScope {
    id: container
    focus: true

    property bool organized: false

    function stop() {
        container.organized = false;
    }

    Repeater {
        model: KWinComponents.ClientFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: KWinComponents.ClientFilterModel.Desktop
        }

        KWinComponents.WindowThumbnailItem {
            id: windowThumbnail
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            width: model.client.width
            height: model.client.height
        }
    }

    Rectangle {
        id: underlay
        anchors.fill: parent
        color: PlasmaCore.ColorScope.backgroundColor
        state: container.organized ? "active" : "initial"

        states: [
            State {
                name: "initial"
                PropertyChanges {
                    target: underlay
                    opacity: 0
                }
            },
            State {
                name: "active"
                PropertyChanges {
                    target: underlay
                    opacity: 0.4
                }
            }
        ]

        Behavior on opacity {
            OpacityAnimator { duration: effect.animationDuration }
        }
    }

    WindowHeap {
        anchors.fill: parent
        focus: true
        organized: container.organized
        model: KWinComponents.ClientFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: ~KWinComponents.ClientFilterModel.Dock &
                    ~KWinComponents.ClientFilterModel.Desktop &
                    ~KWinComponents.ClientFilterModel.Notification;
        }
    }

    Repeater {
        model: KWinComponents.ClientFilterModel {
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: KWinComponents.ClientFilterModel.Dock
        }

        KWinComponents.WindowThumbnailItem {
            id: windowThumbnail
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            width: model.client.width
            height: model.client.height
        }
    }

    KWinComponents.ClientModel {
        id: stackModel
    }

    Component.onCompleted: {
        container.organized = true;
    }
}
