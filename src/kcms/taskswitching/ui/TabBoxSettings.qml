/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols as KQuickControls

import org.kde.kwin.kcmtaskswitching as TaskSwitching

Kirigami.FormLayout {
    id: root

    required property bool isAlternative

    readonly property var tabBoxSettings: isAlternative ? kcm.tabBoxAlternativeSettings : kcm.tabBoxSettings

    Item {
        Kirigami.FormData.label: i18nc("@title:group", "Appearance")
        Kirigami.FormData.isSection: true
    }

    RowLayout {
        spacing: Kirigami.Units.smallSpacing
        Kirigami.FormData.label: i18nc("Task switcher style", "Switcher style:")
        Kirigami.FormData.buddyFor: layoutNameComboBox

        TabBoxLayoutsComboBox {
            id: layoutNameComboBox
            Layout.minimumWidth: Kirigami.Units.gridUnit * 12

            tabBoxSettings: root.tabBoxSettings

            KCM.SettingStateProxy {
                id: layoutNameComboBoxShowTabBoxState
                configObject: root.tabBoxSettings
                settingName: "ShowTabBox"
            }

            KCM.SettingStateProxy {
                id: layoutNameComboBoxLayoutNameState
                configObject: root.tabBoxSettings
                settingName: "LayoutName"
            }

            KCM.SettingHighlighter {
                highlight: !layoutNameComboBoxShowTabBoxState.defaulted || !layoutNameComboBoxLayoutNameState.defaulted
            }
        }

        QQC2.Button {
            icon.name: "view-preview-symbolic"
            text: i18nc("@info:tooltip", "Preview Switcher…")
            display: QQC2.AbstractButton.IconOnly

            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            enabled: root.tabBoxSettings.showTabBox && layoutNameComboBox.path.length > 0

            onClicked: kcm.previewTabBoxLayout(layoutNameComboBox.path, root.tabBoxSettings.showDesktopMode)
        }
    }

    QQC2.CheckBox {
        Kirigami.FormData.label: i18n("Selecting a window:")

        text: i18nc("@option:check", "Hides other windows")

        checked: root.tabBoxSettings.highlightWindows
        onToggled: root.tabBoxSettings.highlightWindows = checked

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "HighlightWindows"
        }
    }

    Item {
        Kirigami.FormData.label: i18nc("@title:group", "Behavior")
        Kirigami.FormData.isSection: true
    }

    QQC2.CheckBox {
        Kirigami.FormData.label: i18nc("As in, 'Include: “Show Desktop” entry'", "Include:")

        text: i18nc("@option:check, 'Include:'", "“Show Desktop” entry")

        checked: root.tabBoxSettings.showDesktopMode == TaskSwitching.tabBoxConfig.ShowDesktopClient
        onToggled: root.tabBoxSettings.showDesktopMode = (checked ? TaskSwitching.tabBoxConfig.ShowDesktopClient : TaskSwitching.tabBoxConfig.DoNotShowDesktopClient)

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "ShowDesktopMode"
        }
    }

    Item {
        Kirigami.FormData.isSection: true
    }

    QQC2.ComboBox {
        id: switchingModeComboBox
        Kirigami.FormData.label: i18n("Sort windows by:")

        Layout.preferredWidth: Math.max(switchingModeComboBox.implicitWidth,
                                        desktopModeComboBox.implicitWidth,
                                        activitiesModeComboBox.implicitWidth,
                                        multiScreenModeComboBox.implicitWidth,
                                        minimizedModeComboBox.implicitWidth)

        // NOTE: Must match enums in tabbox/tabboxconfig.h
        model: [i18nc("@item:inlistbox 'Sort windows by:'", "Recently used" ),
                i18nc("@item:inlistbox 'Sort windows by:'", "Stacking order")]

        currentIndex: root.tabBoxSettings.switchingMode
        onActivated: root.tabBoxSettings.switchingMode = currentIndex

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "SwitchingMode"
        }
    }

    QQC2.CheckBox {
        text: i18nc("@option:check", "Sort minimized windows after unminimized")

        checked: root.tabBoxSettings.orderMinimizedMode == TaskSwitching.tabBoxConfig.GroupByMinimized
        onToggled: root.tabBoxSettings.orderMinimizedMode = (checked ? TaskSwitching.tabBoxConfig.GroupByMinimized : TaskSwitching.tabBoxConfig.NoGroupByMinimized)

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "OrderMinimizedMode"
        }
    }

    Item {
        Kirigami.FormData.isSection: true
    }

    // TODO: Adjust Task Manager settings strings to match, add "the" and move 'screen' to below 'desktop' and 'activity'

    QQC2.ComboBox {
        id: desktopModeComboBox
        Kirigami.FormData.label: i18nc("As in, 'Show windows: From all desktops'", "Show windows:")

        Layout.preferredWidth: Math.max(switchingModeComboBox.implicitWidth,
                                        desktopModeComboBox.implicitWidth,
                                        activitiesModeComboBox.implicitWidth,
                                        multiScreenModeComboBox.implicitWidth,
                                        minimizedModeComboBox.implicitWidth)

        // NOTE: Must match enums in tabbox/tabboxconfig.h
        model: [i18nc("@item:inlistbox 'Show windows:'", "From all desktops"       ),
                i18nc("@item:inlistbox 'Show windows:'", "From the current desktop"),
                i18nc("@item:inlistbox 'Show windows:'", "From all other desktops" )]

        currentIndex: root.tabBoxSettings.desktopMode
        onActivated: root.tabBoxSettings.desktopMode = currentIndex

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "DesktopMode"
        }
    }

    QQC2.ComboBox {
        id: activitiesModeComboBox

        Layout.preferredWidth: Math.max(switchingModeComboBox.implicitWidth,
                                        desktopModeComboBox.implicitWidth,
                                        activitiesModeComboBox.implicitWidth,
                                        multiScreenModeComboBox.implicitWidth,
                                        minimizedModeComboBox.implicitWidth)

        // NOTE: Must match enums in tabbox/tabboxconfig.h
        model: [i18nc("@item:inlistbox 'Show windows:'", "From all activities"      ),
                i18nc("@item:inlistbox 'Show windows:'", "From the current activity"),
                i18nc("@item:inlistbox 'Show windows:'", "From all other activities")]

        currentIndex: root.tabBoxSettings.activitiesMode
        onActivated: root.tabBoxSettings.activitiesMode = currentIndex

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "ActivitiesMode"
        }
    }

    QQC2.ComboBox {
        id: multiScreenModeComboBox

        Layout.preferredWidth: Math.max(switchingModeComboBox.implicitWidth,
                                        desktopModeComboBox.implicitWidth,
                                        activitiesModeComboBox.implicitWidth,
                                        multiScreenModeComboBox.implicitWidth,
                                        minimizedModeComboBox.implicitWidth)

        // NOTE: Must match enums in tabbox/tabboxconfig.h
        model: [i18nc("@item:inlistbox 'Show windows:'", "From all screens"       ),
                i18nc("@item:inlistbox 'Show windows:'", "From the current screen"),
                i18nc("@item:inlistbox 'Show windows:'", "From all other screens" )]

        currentIndex: root.tabBoxSettings.multiScreenMode
        onActivated: root.tabBoxSettings.multiScreenMode = currentIndex

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "MultiScreenMode"
        }
    }

    QQC2.ComboBox {
        id: minimizedModeComboBox

        Layout.preferredWidth: Math.max(switchingModeComboBox.implicitWidth,
                                        desktopModeComboBox.implicitWidth,
                                        activitiesModeComboBox.implicitWidth,
                                        multiScreenModeComboBox.implicitWidth,
                                        minimizedModeComboBox.implicitWidth)

        // NOTE: Must match enums in tabbox/tabboxconfig.h
        model: [i18nc("@item:inlistbox 'Show windows:'", "Regardless of minimization state"),
                i18nc("@item:inlistbox 'Show windows:'", "That are unminimized"  ),
                i18nc("@item:inlistbox 'Show windows:'", "That are minimized"    )]

        currentIndex: root.tabBoxSettings.minimizedMode
        onActivated: root.tabBoxSettings.minimizedMode = currentIndex

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "MinimizedMode"
        }
    }

    QQC2.CheckBox {
        text: i18nc("@option:check", "Only one window per application")

        // TODO: It may make more sense to remove the shortcuts for "Current Application" and instead make the alternative task switcher do that by default
        //       and have this option a combobox also including TaskSwitching.tabBoxConfig.AllWindowsCurrentApplication
        // This would be a challenging config upgrade that would inevitably be incompatible with some configurations

        checked: root.tabBoxSettings.applicationsMode == TaskSwitching.tabBoxConfig.OneWindowPerApplication
        onToggled: root.tabBoxSettings.applicationsMode = (checked ? TaskSwitching.tabBoxConfig.OneWindowPerApplication : TaskSwitching.tabBoxConfig.AllWindowsAllApplications)

        KCM.SettingStateBinding {
            configObject: root.tabBoxSettings
            settingName: "ApplicationsMode"
        }
    }

    Item {
        Kirigami.FormData.isSection: true
    }

    QQC2.Button {
        text: i18n("Configure Shortcuts…")
        icon.name: "preferences-desktop-keyboard-shortcut"
        onClicked: kcm.configureTabBoxShortcuts(root.isAlternative)
    }

    QQC2.Label {
        text: i18nc("@label Hint for task switcher usage", "Use shortcuts to activate the task switcher")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
    }
}
