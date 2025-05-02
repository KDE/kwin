/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

import org.kde.kwin.kcmtaskswitching

Kirigami.FormLayout {

    QQC2.CheckBox {
        id: overviewBox
        Kirigami.FormData.label: i18nc("@label", "Overview:")

        text: i18nc("@option:check, Enable overview effect", "Enable")

        checked: kcm.overviewSettings.overview
        onToggled: kcm.overviewSettings.overview = checked

        KCM.SettingStateBinding {
            configObject: kcm.overviewSettings
            settingName: "Overview"
        }
    }

    QQC2.Label {
        leftPadding: overviewBox.contentItem.leftPadding
        enabled: overviewBox.checked
        text: i18nc("@label Hint for overview effect enable button", "Use shortcuts or hot corners to toggle the overview")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
    }

    Item {
        Kirigami.FormData.isSection: true
    }

    QQC2.CheckBox {
        text: i18nc("@option:check", "Ignore minimized windows")

        checked: kcm.overviewSettings.ignoreMinimized
        onToggled: kcm.overviewSettings.ignoreMinimized = checked

        KCM.SettingStateBinding {
            configObject: kcm.overviewSettings
            settingName: "IgnoreMinimized"
        }
    }

    QQC2.CheckBox {
        text: i18nc("@option:check", "Search results include windows")

        checked: kcm.overviewSettings.filterWindows
        onToggled: kcm.overviewSettings.filterWindows = checked

        KCM.SettingStateBinding {
            configObject: kcm.overviewSettings
            settingName: "FilterWindows"
        }
    }

    QQC2.CheckBox {
        text: i18nc("@option:check", "Organize windows in the Grid view")

        checked: kcm.overviewSettings.organizedGrid
        onToggled: kcm.overviewSettings.organizedGrid = checked

        KCM.SettingStateBinding {
            configObject: kcm.overviewSettings
            settingName: "OrganizedGrid"
        }
    }

    Item {
        Kirigami.FormData.isSection: true
    }

    QQC2.Button {
        text: i18n("Configure Shortcuts…")
        icon.name: "preferences-desktop-keyboard-shortcut"

        enabled: overviewBox.checked

        onClicked: kcm.configureOverviewShortcuts()
    }
}
