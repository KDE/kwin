/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.15

import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin.private.kdecoration 1.0 as KDecoration

ListView {
    id: view
    property string key
    property bool dragging: false
    property int iconSize: Kirigami.Units.iconSizes.small
    orientation: ListView.Horizontal
    interactive: false
    spacing: Kirigami.Units.smallSpacing
    implicitHeight: iconSize
    implicitWidth: count * (iconSize + Kirigami.Units.smallSpacing) - Math.min(1, count) * Kirigami.Units.smallSpacing
    delegate: Item {
        width: view.iconSize
        height: view.iconSize
        KDecoration.Button {
            id: button
            property int itemIndex: index
            property var buttonsModel: parent.ListView.view.model
            bridge: bridgeItem.bridge
            settings: settingsItem
            type: model["button"]
            width: view.iconSize
            height: view.iconSize
            anchors.fill: Drag.active ? undefined : parent
            Drag.keys: [ "decoButtonRemove", view.key ]
            Drag.active: dragArea.drag.active
            Drag.onActiveChanged: view.dragging = Drag.active
            color: palette.windowText
            opacity: parent.enabled ? 1.0 : 0.3
        }
        MouseArea {
            id: dragArea
            cursorShape: Qt.SizeAllCursor
            anchors.fill: parent
            drag.target: button
            onReleased: {
                if (drag.target.Drag.target) {
                    drag.target.Drag.drop();
                } else {
                    drag.target.Drag.cancel();
                }
            }
        }
    }
    add: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: Kirigami.Units.longDuration/2 }
        NumberAnimation { property: "scale"; from: 0; to: 1.0; duration: Kirigami.Units.longDuration/2 }
    }
    move: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: Kirigami.Units.longDuration/2 }
        NumberAnimation { property: "scale"; from: 0; to: 1.0; duration: Kirigami.Units.longDuration/2 }
    }
    displaced: Transition {
        NumberAnimation { properties: "x,y"; duration: Kirigami.Units.longDuration; easing.type: Easing.OutBounce }
    }
}
