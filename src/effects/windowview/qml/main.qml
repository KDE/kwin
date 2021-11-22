/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtGraphicalEffects 1.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: container

    required property QtObject effect
    required property QtObject targetScreen
    required property var selectedIds

    property bool animationEnabled: false
    property bool organized: false

    readonly property int animationDuration: PlasmaCore.Units.longDuration

    function start() {
        container.animationEnabled = true;
        container.organized = true;
    }

    function stop() {
        container.organized = false;
    }

    Keys.onEscapePressed: effect.deactivate(animationDuration);

    KWinComponents.DesktopBackgroundItem {
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentVirtualDesktop
        outputName: targetScreen.name

        layer.enabled: true
        layer.effect: FastBlur {
            radius: container.organized ? 64 : 0
            Behavior on radius {
                NumberAnimation { duration: effect.animationDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: PlasmaCore.ColorScope.backgroundColor
        opacity: container.organized ? 0.75 : 0

        TapHandler {
            onTapped: effect.deactivate(animationDuration);
        }

        Behavior on opacity {
            OpacityAnimator { duration: animationDuration; easing.type: Easing.OutCubic }
        }
    }

    WindowHeap {
        width: targetScreen.geometry.width
        height: targetScreen.geometry.height
        focus: true
        padding: PlasmaCore.Units.largeSpacing
        animationDuration: container.animationDuration
        animationEnabled: container.animationEnabled
        dragEnabled: false
        organized: container.organized
        showOnly: selectedIds
        model: KWinComponents.ClientFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: KWinComponents.Workspace.currentVirtualDesktop
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: ~KWinComponents.ClientFilterModel.Dock &
                    ~KWinComponents.ClientFilterModel.Desktop &
                    ~KWinComponents.ClientFilterModel.Notification;
        }
        onActivated: effect.deactivate(animationDuration);
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
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            visible: opacity > 0
            opacity: (model.client.hidden || container.organized) ? 0 : 1

            Behavior on opacity {
                NumberAnimation { duration: animationDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    KWinComponents.ClientModel {
        id: stackModel
    }

    Component.onCompleted: start();
}
