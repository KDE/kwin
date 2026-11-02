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
    id: root
    Kirigami.FormGroup {
        Kirigami.FormEntry {
            title: i18nc("@label", "Overview:")
            subtitle: {
                if (kcm.overviewShortcuts.length === 0) {
                    return i18nc("@label Hint for overview effect enable button", "Swipe up with four fingers on a touchpad or three on a touchscreen, or use screen edge gestures to toggle the overview");
                } else if (kcm.overviewShortcuts.length === 1) {
                    return i18nc("@label Hint for overview effect enable button, %1 is a keyboard shortcut", "Press %1, swipe up with four fingers on a touchpad or three on a touchscreen, or use screen edge gestures to toggle the overview", kcm.overviewShortcuts[0]);
                }
                return i18nc("@label Hint for overview effect enable button, %1 and %2 are alternative keyboard shortcuts", "Press %1 or %2, swipe up with four fingers on a touchpad or three on a touchscreen, or use screen edge gestures to toggle the overview", kcm.overviewShortcuts[0], kcm.overviewShortcuts[1]);
            }
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
                onTriggered: kcm.configureOverviewShortcuts(root)
            }
        }

        Kirigami.FormSeparator {}

        Kirigami.FormAction {
            enabled: action.enabled
            action: QQC2.Action {
                text: i18nc("@action:button", "Configure Screen Edges…")
                icon.name: "preferences-desktop-gestures-screenedges"
                enabled: overviewBox.checked
                onTriggered: KCM.KCMLauncher.openSystemSettings("kcm_kwinscreenedges")
            }
        }

        Kirigami.FormSeparator {}

        Kirigami.FormAction {
            enabled: action.enabled
            action: QQC2.Action {
                text: i18nc("@action:button", "Configure Touchscreen Gestures…")
                icon.name: "preferences-desktop-gestures-touch"
                enabled: overviewBox.checked
                onTriggered: KCM.KCMLauncher.openSystemSettings("kcm_kwintouchscreen")
            }
        }
    }
}
