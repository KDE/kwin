/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kwin.kwinxwaylandsettings
import org.kde.kquickcontrols

KCM.SimpleKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 48
    implicitHeight: Kirigami.Units.gridUnit * 33

    ColumnLayout {
        id: column

        QQC2.Label {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            text: xi18nc("@info:usagetip", "Legacy X11 apps with global shortcuts and other features accessed while running in the background need to be able to listen for keystrokes all the time.<nl/><nl/>If you use any of these apps, you can choose your preferred balance of security and compatibility here.")
            wrapMode: Text.Wrap
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit
        }

        Kirigami.Form {
            id: keySnoopingForm

            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit
            Layout.fillWidth: true

            QQC2.ButtonGroup { id: keyboardSnoopingGroup }

            Kirigami.FormGroup {
                Kirigami.FormEntry {
                    title: i18nc("@title:group", "Listening for keystrokes:")
                    contentItem: QQC2.RadioButton {
                        id: prohibited
                        text: i18nc("@option:radio Listening for keystrokes is prohibited", "Prohibited")
                        QQC2.ButtonGroup.group: keyboardSnoopingGroup
                        checked: kcm.settings.xwaylandEavesdrops === 0
                        onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 0
                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "xwaylandEavesdrops"
                        }
                    }
                    subtitle: i18nc("@info:usagetip", "Most secure; global shortcuts will not work in X11 apps")
                }

                Kirigami.FormEntry {
                    contentItem:  QQC2.RadioButton {
                        id: modifierKeysOnly
                        text: i18nc("@option:radio Listening for keystrokes is allowed for…", "Only the Meta, Control, Alt, and Shift keys")
                        QQC2.ButtonGroup.group: keyboardSnoopingGroup
                        checked: kcm.settings.xwaylandEavesdrops === 1
                        onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 1
                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "xwaylandEavesdrops"
                        }
                    }
                    subtitle: i18nc("@info:usagetip", "High security; push-to-talk and other modifier-only global shortcuts will work in X11 apps")
                }
                Kirigami.FormEntry {
                    contentItem:  QQC2.RadioButton {
                        id: charsWithModifierKeys
                        text: i18nc("@option:radio Listening for keystrokes is allowed for…", "As above, plus any key pressed while the Control, Alt, or Meta key is also pressed")
                        QQC2.ButtonGroup.group: keyboardSnoopingGroup
                        checked: kcm.settings.xwaylandEavesdrops === 2
                        onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 2
                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "xwaylandEavesdrops"
                        }
                    }
                    subtitle: i18nc("@info:usagetip", "Moderate security; all global shortcuts will work in X11 apps")
                }

                Kirigami.FormEntry {
                    contentItem: QQC2.RadioButton {
                        id: always
                        text: i18nc("@option:radio Listening for keystrokes is always allowed","Always allowed")
                        QQC2.ButtonGroup.group: keyboardSnoopingGroup
                        checked: kcm.settings.xwaylandEavesdrops === 3
                        onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 3
                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "xwaylandEavesdrops"
                        }
                    }
                    subtitle: i18nc("@info:usagetip", "Least secure; all X11 apps will be able to see any text you type into any application")
                }
                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.gridUnit
                    type: Kirigami.MessageType.Warning
                    text: i18nc("@info:usagetip", "Note that using this setting will reduce system security and permit malicious software to steal passwords and spy on the text that you type. Make sure you understand and accept this risk.")
                    visible: always.checked
                }
            }

            Kirigami.FormGroup {
                Kirigami.FormEntry {
                    title: i18nc("@title:group", "Listening for mouse buttons:")
                    contentItem: QQC2.CheckBox {
                        id: mouseButtonSnooping
                        text: i18nc("@option:radio Listening for mouse buttons is allowed","Allowed")
                        checked: kcm.settings.xwaylandEavesdropsMouse
                        onToggled: kcm.settings.xwaylandEavesdropsMouse = checked
                        enabled: !prohibited.checked
                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "xwaylandEavesdropsMouse"
                        }
                    }
                    subtitle: i18nc("@info:usagetip", "Moderate security; global shortcuts involving mouse buttons will work in X11 apps")

                }
            }

            QQC2.ButtonGroup {
                id: xwaylandGroup
            }

            Kirigami.FormGroup {
                Kirigami.FormEntry {
                    title: i18nc("@title:group", "Control of pointer and keyboard:")
                    contentItem: RowLayout {
                        Kirigami.FormData.buddyFor: allowedApps
                        QQC2.RadioButton {
                            id: allowedApps
                            text:  i18nc("@option:check Allowed control of pointer and keyboard without asking for permissions for allowed apps", "Allowed apps")
                            QQC2.ButtonGroup.group: xwaylandGroup
                            checked: !kcm.settings.xwaylandEisNoPrompt
                            onToggled: kcm.settings.xwaylandEisNoPrompt = !checked
                        }
                        QQC2.Button {
                            text: i18nc("@action:button", "See Allowed Applications…")
                            enabled: allowedApps.checked
                            onClicked: {
                                kcm.push(appsPage)
                            }
                        }
                        Item {
                            // Try to get some more space as we have a very long subtitle.
                            Layout.fillWidth: true
                        }
                    }
                    subtitle: i18nc("@info:usagetip", "Moderate security; XWayland-using apps claiming to be something on this list will be able to take control of the computer, because XWayland-using apps’ identities can’t be verified")
                }

                Kirigami.FormEntry {
                    contentItem: QQC2.RadioButton {
                        id: totalControl
                        text: i18nc("@option:radio Allow control of pointer and keyboard without asking for permission", "Allow without asking for permission")
                        QQC2.ButtonGroup.group: xwaylandGroup
                        checked: kcm.settings.xwaylandEisNoPrompt
                        onToggled: kcm.settings.xwaylandEisNoPrompt = checked
                    }
                    subtitle: i18nc("@info:usagetip", "Least secure; all X11 apps will be able to take control of the computer")
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.gridUnit
                    type: Kirigami.MessageType.Warning
                    text: i18nc("@info:usagetip", "Note that using this setting will drastically reduce system security and permit malicious software to take complete control of your pointer and keyboard. Make sure you understand and accept this risk.")
                    visible: totalControl.checked
                }
            }
        }
        KCM.ScrollViewKCM {
            id: appsPage
            visible: false
            title: i18nc("@title:window title of the page opened by the 'See allowed applications' button", "Applications allowed to control the pointer and keyboard")
            view: ListView {
                model: kcm.settings.xwaylandEisNoPromptApps
                delegate: QQC2.ItemDelegate {
                    id: delegate
                    width: ListView.view.width
                    text: modelData
                    icon.name: modelData
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.IconTitleSubtitle {
                            Layout.fillWidth: true
                            icon: icon.fromControlsIcon(delegate.icon)
                            title: delegate.text
                            selected: delegate.highlighted || delegate.down
                            font: delegate.font
                        }
                        QQC2.ToolButton {
                            icon.name: "list-remove-symbolic"
                            display: QQC2.AbstractButton.IconOnly
                            text: i18nc("@info:tooltip %1 is the name of the app/binary", "Do not allow %1 to control the pointer and keyboard without asking", modelData)
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: kcm.settings.xwaylandEisNoPromptApps = kcm.settings.xwaylandEisNoPromptApps.filter(app => app != modelData)
                        }
                    }
                }
            }
        }
    }
}
