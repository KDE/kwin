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
    KCM.SettingStateBinding {
        configObject: kcm.settings
        settingName: "Xwayland"
    }
    implicitWidth: Kirigami.Units.gridUnit * 48
    implicitHeight: Kirigami.Units.gridUnit * 33

    ColumnLayout {
        id: column
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            text: i18n("Some legacy X11 apps require the ability to read keystrokes typed in other apps for certain features, such as handling global keyboard shortcuts. This is allowed by default. However other features may require the ability to read all keys, and this is disabled by default for security reasons. If you need to use such apps, you can choose your preferred balance of security and functionality here.")
            wrapMode: Text.Wrap
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit
        }

        Kirigami.FormLayout {
            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.gridUnit

            QQC2.RadioButton {
                id: never
                Kirigami.FormData.label: i18n("Allow legacy X11 apps to read keystrokes typed in all apps:")
                text: i18n("Never")
                checked: kcm.settings.xwaylandEavesdrops === 0
                onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 0
            }

            QQC2.RadioButton {
                text: i18n("Only Meta, Control, Alt and Shift keys")
                checked: kcm.settings.xwaylandEavesdrops === 1
                onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 1
            }

            QQC2.RadioButton {
                text: i18n("As above, plus any key typed while the Control, Alt, or Meta keys are pressed")
                checked: kcm.settings.xwaylandEavesdrops === 2
                onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 2
            }

            QQC2.RadioButton {
                id: always
                text: i18n("Always")
                checked: kcm.settings.xwaylandEavesdrops === 3
                onToggled: if (checked) kcm.settings.xwaylandEavesdrops = 3
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            QQC2.CheckBox {
                text: i18n("Additionally include mouse buttons")
                checked: kcm.settings.xwaylandEavesdropsMouse
                onToggled: kcm.settings.xwaylandEavesdropsMouse = checked
                enabled: !never.checked
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            type: Kirigami.MessageType.Warning
            text: i18n("Note that using this setting will reduce system security to that of the X11 session by permitting malicious software to steal passwords and spy on the text that you type. Make sure you understand and accept this risk.")
            visible: always.checked
        }
    }
}
