/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import org.kde.kwin 2.0 as KWinComponents

Item {
    id: root

    property int desktop: 1

    // These unused properties exist for compatibility reasons.
    property real brightness: 1
    property real saturation: 1
    property Item clipTo: null

    Item {
        id: container
        anchors.centerIn: parent
        width: workspace.virtualScreenSize.width
        height: workspace.virtualScreenSize.height
        scale: Math.min(parent.width / container.width, parent.height / container.height)

        Repeater {
            model: KWinComponents.ClientModel {
                exclusions: KWinComponents.ClientModel.OtherActivitiesExclusion
            }

            KWinComponents.ThumbnailItem {
                client: model.client
                x: model.client.x
                y: model.client.y
                z: model.client.stackingOrder
                visible: (model.client.desktop === -1 || model.client.desktop === root.desktop) && !model.client.minimized
            }
        }
    }
}
