/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4 as Controls
import org.kde.kcm 1.1 as KCM
import org.kde.kconfig 1.0 // for KAuthorized
import org.kde.kirigami 2.4 as Kirigami
import org.kde.kwin.private.kdecoration 1.0 as KDecoration


KCM.GridViewKCM {
    KCM.ConfigModule.quickHelp: i18n("This module lets you configure the window decorations.")

    SystemPalette {
        id: palette
        colorGroup: SystemPalette.Active
    }

    implicitWidth: Kirigami.Units.gridUnit * 48
    implicitHeight: Kirigami.Units.gridUnit * 33

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
    view.onContentHeightChanged: view.positionViewAtIndex(view.currentIndex, GridView.Visible)

    view.implicitCellWidth: Kirigami.Units.gridUnit * 18

    view.delegate: KCM.GridDelegate {
        id: delegate
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
                onClicked: {
                    kcm.theme = index
                    view.currentIndex = index
                }
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
                    kcm.theme = index
                    view.currentIndex = index
                    bridgeItem.bridge.configure(delegate)
                }
            }
        ]

        onClicked: {
            kcm.theme = index
            view.currentIndex = index
        }
    }

    Connections {
        target: kcm
        onThemeChanged: view.currentIndex = kcm.theme
    }


    view.enabled: !kcm.settings.isImmutable("pluginName") && !kcm.settings.isImmutable("theme")

    footer: RowLayout {

        Controls.CheckBox {
            id: borderSizeAutoCheckbox
            // Let it elide but don't make it push the ComboBox away from it
            Layout.fillWidth: true
            Layout.maximumWidth: implicitWidth
            text: i18nc("checkbox label", "Use theme's default window border size")
            enabled: !kcm.settings.isImmutable("borderSizeAuto")
            checked: kcm.settings.borderSizeAuto
            onToggled: {
                kcm.settings.borderSizeAuto = checked;
                borderSizeComboBox.autoBorderUpdate()
            }
        }
        Controls.ComboBox {
            id: borderSizeComboBox
            enabled: !borderSizeAutoCheckbox.checked && !kcm.settings.isImmutable("borderSize")
            model: kcm.borderSizesModel
            currentIndex: kcm.borderSize
            onActivated: {
                kcm.borderSize = currentIndex
            }
            function autoBorderUpdate() {
                if (borderSizeAutoCheckbox.checked) {
                    kcm.borderSize = kcm.recommendedBorderSize
                }
            }

            Connections {
                target: kcm
                onThemeChanged: borderSizeComboBox.autoBorderUpdate()
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Controls.Button {
            text: i18nc("button text", "Configure buttons...")
            icon.name: "configure"
            onClicked: kcm.push("ButtonConfig.qml")
        }

        Controls.Button {
            text: i18nc("button text", "Get New Window Decorations...")
            icon.name: "get-hot-new-stuff"
            onClicked: kcm.getNewStuff(this)
            visible: KAuthorized.authorize("ghns")
        }
    }
}
