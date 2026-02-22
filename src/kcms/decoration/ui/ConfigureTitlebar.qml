/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>
    SPDX-FileCopyrightText: 2023 Joshua Goins <josh@redstrate.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

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
            text: i18nc("checkbox label", "Close windows by double clicking the window menu button")
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
            text: i18nc("popup tip", "Click and hold on the window menu button to show the menu.")
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
        RowLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.CheckBox {
                id: alwaysShowExcludeFromCaptureCheckBox
                text: i18nc("@option:check", "Always show \"Hide from Screencast\" button")
                checked: kcm.settings.alwaysShowExcludeFromCapture
                onToggled: kcm.settings.alwaysShowExcludeFromCapture = checked

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "alwaysShowExcludeFromCapture"
                    extraEnabledConditions: kcm.excludeFromCaptureButtonSelected
                }
            }

            Kirigami.ContextualHelpButton {
                readonly property string helpText: i18nc("@info:tooltip", "When unchecked the button only appears if the window is hidden from screencast")
                readonly property string additionalHelpText: i18nc("@info:tooltip", "To enable this option, add the \"Hide from screencast\" button to the titlebar first")
                toolTipText: alwaysShowExcludeFromCaptureCheckBox.enabled
                    ? helpText
                    : helpText + "\n\n" + additionalHelpText
            }
        }
    }
}
