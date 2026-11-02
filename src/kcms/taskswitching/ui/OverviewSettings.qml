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

Kirigami.Form {
    Kirigami.FormGroup {
        Kirigami.FormEntry {
            title: i18nc("@label", "Overview:")
            subtitle: i18nc("@label Hint for overview effect enable button", "Use shortcuts or hot corners to toggle the overview")
            contentItem: QQC2.CheckBox {
                id: overviewBox

                text: i18nc("@option:check, Enable overview effect", "Enable")

                checked: kcm.overviewSettings.overview
                onToggled: kcm.overviewSettings.overview = checked

                KCM.SettingStateBinding {
                    configObject: kcm.overviewSettings
                    settingName: "Overview"
                }
            }
        }
    }

    Kirigami.FormGroup {
        Kirigami.FormEntry {
            contentItem: QQC2.CheckBox {
                text: i18nc("@option:check", "Show minimized windows")

                // Intentionally inverted, so the unchecked state is not a double negative

                checked: !kcm.overviewSettings.ignoreMinimized
                onToggled: kcm.overviewSettings.ignoreMinimized = !checked

                KCM.SettingStateBinding {
                    configObject: kcm.overviewSettings
                    settingName: "IgnoreMinimized"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.CheckBox {
                text: i18nc("@option:check", "Search results include windows")

                checked: kcm.overviewSettings.filterWindows
                onToggled: kcm.overviewSettings.filterWindows = checked

                KCM.SettingStateBinding {
                    configObject: kcm.overviewSettings
                    settingName: "FilterWindows"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.CheckBox {
                text: i18nc("@option:check", "Organize windows in the Grid view")

                checked: kcm.overviewSettings.organizedGrid
                onToggled: kcm.overviewSettings.organizedGrid = checked

                KCM.SettingStateBinding {
                    configObject: kcm.overviewSettings
                    settingName: "OrganizedGrid"
                }
            }
        }
    }

    Kirigami.FormGroup {
        Kirigami.FormAction {
            enabled: action.enabled
            action: QQC2.Action {
                text: i18nc("@action:button", "Configure Shortcuts…")
                icon.name: "preferences-desktop-keyboard-shortcut"
                enabled: overviewBox.checked
                onTriggered: kcm.configureOverviewShortcuts()
            }
        }
    }
}
