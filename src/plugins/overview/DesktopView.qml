/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects

Item {
    id: desktopView

    required property KWinComponents.WindowModel windowModel
    required property KWinComponents.VirtualDesktop desktop

    Repeater {
        model: KWinComponents.WindowFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            screenName: targetScreen.name
            windowModel: desktopView.windowModel
        }

        Item {
            x: model.window.x - targetScreen.geometry.x
            y: model.window.y - targetScreen.geometry.y
            width: model.window.width
            height: model.window.height
            z: model.window.stackingOrder
            visible: !model.window.minimized

            KWinComponents.WindowThumbnail {
                anchors.fill: parent
                wId: model.window.internalId
                visible: !placeholder.active
            }

            CaptureExcludedPlaceholder {
                id: placeholder
                anchors.fill: parent
                window: model.window
            }
        }
    }
}
