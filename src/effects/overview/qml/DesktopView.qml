/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import org.kde.kwin 3.0 as KWinComponents

Item {
    id: desktopView

    required property QtObject windowModel
    required property QtObject desktop

    Repeater {
        model: KWinComponents.WindowFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            screenName: targetScreen.name
            windowModel: desktopView.windowModel
        }

        KWinComponents.WindowThumbnailItem {
            wId: model.window.internalId
            x: model.window.x - targetScreen.geometry.x
            y: model.window.y - targetScreen.geometry.y
            width: model.window.width
            height: model.window.height
            z: model.window.stackingOrder
            visible: !model.window.minimized
        }
    }
}
