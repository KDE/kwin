/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>
    SPDX-FileCopyrightText: 2023 Joshua Goins <josh@redstrate.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kcm as KCM
import org.kde.kirigami 2.20 as Kirigami
import org.kde.newstuff as NewStuff

KCM.AbstractKCM {
    id: root

    KCM.ConfigModule.quickHelp: i18n("This module lets you configure the window decorations.")
    title: kcm.name

    framedView: false

    implicitWidth: Kirigami.Units.gridUnit * 48
    implicitHeight: Kirigami.Units.gridUnit * 33

    Themes {
        id: themes

        anchors.fill: parent


        KCM.SettingStateBinding {
            target: themes
            configObject: kcm.settings
            settingName: "pluginName"
        }
    }

    footer: RowLayout {
        QQC2.Label {
            text: i18nc("Selector label", "Window border size:")
        }

        QQC2.ComboBox {
            id: borderSizeComboBox
            model: kcm.borderSizesModel
            currentIndex: kcm.borderIndex
            onActivated: kcm.borderIndex = currentIndex;
            KCM.SettingHighlighter {
                highlight: kcm.borderIndex !== 0
            }
        }

        Kirigami.ActionToolBar {
            flat: false
            alignment: Qt.AlignRight
            actions: [
                Kirigami.Action {
                    text: i18nc("button text", "Configure Titlebar Buttons…")
                    icon.name: "configure"
                    onTriggered: kcm.push("ConfigureTitlebar.qml")
                },
                NewStuff.Action {
                    text: i18nc("button text", "Get New Window Decorations…")
                    configFile: "window-decorations.knsrc"
                    onEntryEvent: (entry, event) => {
                        if (event === NewStuff.Engine.StatusChangedEvent) {
                            kcm.reloadKWinSettings();
                        } else if (event === NewStuff.Engine.EntryAdoptedEvent) {
                            kcm.load();
                        }
                    }
                }
            ]
        }
    }
}
