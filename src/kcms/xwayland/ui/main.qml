/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami 2.6 as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kwin.kwinxwaylandsettings
import org.kde.kquickcontrols

KCM.SimpleKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 48
    implicitHeight: Kirigami.Units.gridUnit * 33

     header: Kirigami.InlineMessage {
        id: takeEffectNextTimeMsg
        Layout.fillWidth: true
        type: Kirigami.MessageType.Information
        position: Kirigami.InlineMessage.Position.Header
        text: i18nc("@info", "Changes will take effect the next time you log in.")
        actions: [
            Kirigami.Action {
                icon.name: "system-log-out-symbolic"
                text: i18nc("@action:button", "Log Out Now")
                onTriggered: {
                    kcm.logout()
                }
            }
        ]
        Connections {
            target: kcm
            function onShowLogoutMessage() {
                takeEffectNextTimeMsg.visible = true;
            }
        }
    }

    ColumnLayout {
        id: column
        spacing: 0

        QQC2.Label {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            text: xi18nc("@info", "Legacy X11 apps require broad permissions for their global shortcuts and other features accessed while in other apps, which can reduce system security.<nl/><nl/>Here you can choose your preferred balance of security and compatibility for such apps.")
            wrapMode: Text.Wrap
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit
        }

        Kirigami.FormLayout {
            id: keySnoopingLayout

            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit

            twinFormLayouts: bottomLayout

            QQC2.ButtonGroup { id: keyboardSnoopingGroup }

            ColumnLayout {
                Kirigami.FormData.label: i18n("Keyboard snooping:")
                Kirigami.FormData.buddyFor: prohibited
                Layout.fillWidth: true
                spacing : 0

                QQC2.RadioButton {
                    id: prohibited
                    text: i18n("Prohibited")
                    QQC2.ButtonGroup.group: keyboardSnoopingGroup
                    checked: kcm.settings.xwaylandEavesdrops === 0
                    onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 0

                    KCM.SettingStateBinding {
                        configObject: kcm.settings
                        settingName: "xwaylandEavesdrops"
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    leftPadding: Application.layoutDirection === Qt.LeftToRight ?
                    prohibited.indicator.width + prohibited.spacing : padding
                    rightPadding: Application.layoutDirection === Qt.RightToLeft ?
                    prohibited.indicator.width + prohibited.spacing : padding
                    text: i18n("Most secure; no global shortcuts will work in X11 apps ")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing : 0
                QQC2.RadioButton {
                    id: modifierKeysOnly
                    text: i18n("Allowed for modifier keys (Meta, Control, Alt, and Shift)")
                    QQC2.ButtonGroup.group: keyboardSnoopingGroup
                    checked: kcm.settings.xwaylandEavesdrops === 1
                    onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 1

                    KCM.SettingStateBinding {
                        configObject: kcm.settings
                        settingName: "xwaylandEavesdrops"
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    leftPadding: Application.layoutDirection === Qt.LeftToRight ?
                    modifierKeysOnly.indicator.width + modifierKeysOnly.spacing : padding
                    rightPadding: Application.layoutDirection === Qt.RightToLeft ?
                    modifierKeysOnly.indicator.width + modifierKeysOnly.spacing : padding
                    text: i18n("High security; push-to-talk and other modifier-only global shortcuts will work in X11 apps")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing : 0
                QQC2.RadioButton {
                    id: charsWithModifierKeys
                    text: i18n("Allowed for any key typed while a modifier key is pressed")
                    QQC2.ButtonGroup.group: keyboardSnoopingGroup
                    checked: kcm.settings.xwaylandEavesdrops === 2
                    onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 2

                    KCM.SettingStateBinding {
                        configObject: kcm.settings
                        settingName: "xwaylandEavesdrops"
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    leftPadding: Application.layoutDirection === Qt.LeftToRight ?
                    charsWithModifierKeys.indicator.width + charsWithModifierKeys.spacing : padding
                    rightPadding: Application.layoutDirection === Qt.RightToLeft ?
                    charsWithModifierKeys.indicator.width + charsWithModifierKeys.spacing : padding
                    text: i18n("Moderate security; all global shortcuts will work in X11 apps")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing : 0
                QQC2.RadioButton {
                    id: always
                    text: i18n("Always allowed")
                    QQC2.ButtonGroup.group: keyboardSnoopingGroup
                    checked: kcm.settings.xwaylandEavesdrops === 3
                    onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 3

                    KCM.SettingStateBinding {
                        configObject: kcm.settings
                        settingName: "xwaylandEavesdrops"
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    leftPadding: Application.layoutDirection === Qt.LeftToRight ?
                    always.indicator.width + always.spacing : padding
                    rightPadding: Application.layoutDirection === Qt.RightToLeft ?
                    always.indicator.width + always.spacing : padding
                    text: i18n("Very insecure; all X11 apps can spy on any text you type")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.8
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            type: Kirigami.MessageType.Warning
            text: i18n("Note that using this setting will reduce system security to that of the X11 session by permitting malicious software to steal passwords and spy on the text that you type. Make sure you understand and accept this risk.")
            visible: always.checked
        }
        Kirigami.FormLayout {
            id: bottomLayout

            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit

            twinFormLayouts: keySnoopingLayout

            Item {
                Kirigami.FormData.isSection: true
            }

            ColumnLayout {
                Kirigami.FormData.label: i18n("Mouse button snooping:")
                Kirigami.FormData.buddyFor: mouseButtonSnooping
                Layout.fillWidth: true
                spacing: 0
                QQC2.CheckBox {
                    id: mouseButtonSnooping
                    text: i18n("Always allowed")
                    checked: kcm.settings.xwaylandEavesdropsMouse
                    onToggled: kcm.settings.xwaylandEavesdropsMouse = checked
                    enabled: !prohibited.checked

                    KCM.SettingStateBinding {
                        configObject: kcm.settings
                        settingName: "xwaylandEavesdropsMouse"
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    leftPadding: Application.layoutDirection === Qt.LeftToRight ?
                    mouseButtonSnooping.indicator.width + mouseButtonSnooping.spacing : padding
                    rightPadding: Application.layoutDirection === Qt.RightToLeft ?
                    mouseButtonSnooping.indicator.width + mouseButtonSnooping.spacing : padding
                    text: i18n("Moderate security; global shortcuts involving mouse buttons will work in X11 apps")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.8
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            ColumnLayout {
                Kirigami.FormData.label: i18nc("@title:group", "Control of pointer and keyboard:")
                Kirigami.FormData.buddyFor: totalControl
                Layout.fillWidth: true
                spacing : 0
                QQC2.CheckBox {
                    id: totalControl
                    text: i18nc("@option:check Allow control of pointer and keyboard without asking for permission", "Allow without asking for permission")
                    checked: kcm.settings.xwaylandEisNoPrompt
                    onToggled: kcm.settings.xwaylandEisNoPrompt = checked
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    leftPadding: Application.layoutDirection === Qt.LeftToRight ?
                    totalControl.indicator.width + totalControl.spacing : padding
                    rightPadding: Application.layoutDirection === Qt.RightToLeft ?
                    totalControl.indicator.width + totalControl.spacing : padding
                    text: i18n("Very insecure; allows X11 apps to take control of the computer")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    opacity: 0.8
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            type: Kirigami.MessageType.Warning
            text: i18n("Note that using this setting will drastically reduce system security to that of the X11 session by permitting malicious software to take complete control of your pointer and keyboard. Make sure you understand and accept this risk.")
            visible: totalControl.checked
        }
    }
}
