/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>
    SPDX-FileCopyrightText: 2023 Joshua Goins <josh@redstrate.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kcmutils as KCM
import org.kde.kirigami 2.20 as Kirigami

KCM.AbstractKCM {
    title: i18n("Titlebar Buttons")

    framedView: false

    Rectangle {
        anchors.fill: parent
        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        color: Kirigami.Theme.backgroundColor

        Buttons {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
        }
    }

    footer: ColumnLayout {
        QQC2.CheckBox {
            id: closeOnDoubleClickOnMenuCheckBox
            text: i18nc("checkbox label", "Close windows by double clicking the menu button")
            checked: kcm.settings.closeOnDoubleClickOnMenu
            onToggled: {
                kcm.settings.closeOnDoubleClickOnMenu = checked;
                infoLabel.visible = checked;
            }

            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "closeOnDoubleClickOnMenu"
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            id: infoLabel
            type: Kirigami.MessageType.Information
            text: i18nc("popup tip", "Click and hold on the menu button to show the menu.")
            showCloseButton: true
            visible: false
        }

        QQC2.CheckBox {
            id: showToolTipsCheckBox
            text: i18nc("checkbox label", "Show titlebar button tooltips")
            checked: kcm.settings.showToolTips
            onToggled: kcm.settings.showToolTips = checked

            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "showToolTips"
            }
        }
    }
}
