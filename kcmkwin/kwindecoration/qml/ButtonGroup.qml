/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.1
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import org.kde.kwin.private.kdecoration 1.0 as KDecoration
import org.kde.plasma.core 2.0 as PlasmaCore;

ListView {
    id: view
    property string key
    property bool dragging: false
    orientation: ListView.Horizontal
    interactive: false
    spacing: units.smallSpacing
    delegate: Item {
        width: units.iconSizes.small
        height: units.iconSizes.small
        KDecoration.Button {
            id: button
            property int itemIndex: index
            property var buttonsModel: parent.ListView.view.model
            bridge: bridgeItem.bridge
            settings: settingsItem
            type: model["button"]
            anchors.fill: Drag.active ? undefined : parent
            Drag.keys: [ "decoButtonRemove", view.key ]
            Drag.active: dragArea.drag.active
            Drag.onActiveChanged: view.dragging = Drag.active
        }
        MouseArea {
            id: dragArea
            cursorShape: Qt.PointingHandCursor
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
        NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: units.longDuration/2 }
        NumberAnimation { property: "scale"; from: 0; to: 1.0; duration: units.longDuration/2 }
    }
    move: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: units.longDuration/2 }
        NumberAnimation { property: "scale"; from: 0; to: 1.0; duration: units.longDuration/2 }
    }
    displaced: Transition {
        NumberAnimation { properties: "x,y"; duration: units.longDuration; easing.type: Easing.OutBounce }
    }
    Layout.preferredWidth: count * (units.iconSizes.small + units.smallSpacing) - Math.min(1, count) * units.smallSpacing
}
