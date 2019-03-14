/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 * Copyright (c) 2019 Valerio Pilo <vpilo@coldshock.net>
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
import QtQuick 2.7
import org.kde.kcm 1.1 as KCM
import org.kde.kirigami 2.2 as Kirigami
import org.kde.kwin.private.kdecoration 1.0 as KDecoration

KCM.GridView {
    function updateDecoration(item, marginTopLeft, marginBottomRight) {
        var mainMargin = units.largeSpacing
        var shd = item.shadow
        item.anchors.leftMargin   = mainMargin + marginTopLeft     - (shd ? shd.paddingLeft   : 0)
        item.anchors.rightMargin  = mainMargin + marginBottomRight - (shd ? shd.paddingRight  : 0)
        item.anchors.topMargin    = mainMargin + marginTopLeft     - (shd ? shd.paddingTop    : 0)
        item.anchors.bottomMargin = mainMargin + marginBottomRight - (shd ? shd.paddingBottom : 0)
    }

    view.model: kcm.themesModel
    view.currentIndex: kcm.theme
    view.onCurrentIndexChanged: kcm.theme = view.currentIndex
    view.onContentHeightChanged: view.positionViewAtIndex(view.currentIndex, GridView.Visible)

    view.implicitCellWidth: Kirigami.Units.gridUnit * 18

    view.delegate: KCM.GridDelegate {
        text: model.display

        thumbnailAvailable: true
        thumbnail: Rectangle {
            anchors.fill: parent
            color: palette.base
            clip: true

            KDecoration.Bridge {
                id: bridgeItem
                plugin: model.plugin
                theme: model.theme
            }
            KDecoration.Settings {
                id: settingsItem
                bridge: bridgeItem.bridge
            }
            KDecoration.Decoration {
                id: inactivePreview
                bridge: bridgeItem.bridge
                settings: settingsItem
                anchors.fill: parent
                onShadowChanged: updateDecoration(inactivePreview, 0, client.decoration.titleBar.height)
                Component.onCompleted: {
                    client.active = false
                    client.caption = model.display
                    updateDecoration(inactivePreview, 0, client.decoration.titleBar.height)
                }
            }
            KDecoration.Decoration {
                id: activePreview
                bridge: bridgeItem.bridge
                settings: settingsItem
                anchors.fill: parent
                onShadowChanged: updateDecoration(activePreview, client.decoration.titleBar.height, 0)
                Component.onCompleted: {
                    client.active = true
                    client.caption = model.display
                    updateDecoration(activePreview, client.decoration.titleBar.height, 0)
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: view.currentIndex = index
            }
            Connections {
                target: kcm
                onBorderSizeChanged: settingsItem.borderSizesIndex = kcm.borderSize
            }
        }
        actions: [
            Kirigami.Action {
                iconName: "edit-entry"
                tooltip: i18n("Edit %1 Theme", model.display)
                enabled: model.configureable
                onTriggered: {
                    view.currentIndex = index
                    bridgeItem.bridge.configure()
                }
            }
        ]

        onClicked: view.currentIndex = index
    }
    Connections {
        target: kcm
        onThemeChanged: view.currentIndex = kcm.theme
    }
}

