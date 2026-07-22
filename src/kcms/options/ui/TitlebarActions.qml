/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

import org.kde.plasma.kcm.kwinoptions

Kirigami.Form {
    id: root

    readonly property var commandActiveTitlebarModel: [
        { text: i18nd("kcmkwm", "Raise"), value: KWinOptionsSettings.CommandActiveTitlebar1_Raise },
        { text: i18nd("kcmkwm", "Lower"), value: KWinOptionsSettings.CommandActiveTitlebar1_Lower },
        { text: i18nd("kcmkwm", "Raise/lower"), value: KWinOptionsSettings.CommandActiveTitlebar1_ToggleRaiseAndLower },
        { text: i18nd("kcmkwm", "Minimize"), value: KWinOptionsSettings.CommandActiveTitlebar1_Minimize },
        { text: i18nd("kcmkwm", "Close"), value: KWinOptionsSettings.CommandActiveTitlebar1_Close },
        { text: i18nd("kcmkwm", "Window menu"), value: KWinOptionsSettings.CommandActiveTitlebar1_OperationsMenu },
        { text: i18nd("kcmkwm", "Do nothing"), value: KWinOptionsSettings.CommandActiveTitlebar1_Nothing },
    ]

    readonly property var commandInactiveTitlebarModel: [
        { text: i18nd("kcmkwm", "Focus and raise"), value: KWinOptionsSettings.CommandInactiveTitlebar1_ActivateAndRaise },
        { text: i18nd("kcmkwm", "Focus and lower"), value: KWinOptionsSettings.CommandInactiveTitlebar1_ActivateAndLower },
        { text: i18nd("kcmkwm", "Focus"), value: KWinOptionsSettings.CommandInactiveTitlebar1_Activate },
        { text: i18nd("kcmkwm", "Raise"), value: KWinOptionsSettings.CommandInactiveTitlebar1_Raise },
        { text: i18nd("kcmkwm", "Lower"), value: KWinOptionsSettings.CommandInactiveTitlebar1_Lower },
        { text: i18nd("kcmkwm", "Raise/lower"), value: KWinOptionsSettings.CommandInactiveTitlebar1_ToggleRaiseAndLower },
        { text: i18nd("kcmkwm", "Minimize"), value: KWinOptionsSettings.CommandInactiveTitlebar1_Minimize },
        { text: i18nd("kcmkwm", "Close"), value: KWinOptionsSettings.CommandInactiveTitlebar1_Close },
        { text: i18nd("kcmkwm", "Window menu"), value: KWinOptionsSettings.CommandInactiveTitlebar1_OperationsMenu },
        { text: i18nd("kcmkwm", "Do nothing"), value: KWinOptionsSettings.CommandInactiveTitlebar1_Nothing },
    ]

    readonly property var maximizeButtonClickCommandModel: [
        { text: i18nd("kcmkwm", "Maximize"), value: KWinOptionsSettings.MaximizeButtonClickCommand_Maximize },
        { text: i18nd("kcmkwm", "Vertically maximize"), value: KWinOptionsSettings.MaximizeButtonClickCommand_MaximizeVerticalOnly },
        { text: i18nd("kcmkwm", "Horizontally maximize"), value: KWinOptionsSettings.MaximizeButtonClickCommand_MaximizeHorizontalOnly },
    ]

    Kirigami.FormGroup {
        title: i18nd("kcmkwm", "Titlebar Actions")

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "&Double-click:")
            contentItem: QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nd("kcmkwm", "Maximize"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_Maximize },
                    { text: i18nd("kcmkwm", "Vertically maximize"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_MaximizeVerticalOnly },
                    { text: i18nd("kcmkwm", "Horizontally maximize"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_MaximizeHorizontalOnly },
                    { text: i18nd("kcmkwm", "Minimize"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_Minimize },
                    { text: i18nd("kcmkwm", "Lower"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_Lower },
                    { text: i18nd("kcmkwm", "Close"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_Close },
                    { text: i18nd("kcmkwm", "Show on all desktops"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_OnAllDesktops },
                    { text: i18nd("kcmkwm", "Do nothing"), value: KWinOptionsSettings.TitlebarDoubleClickCommand_Nothing },
                ]
                currentValue: kcm.kwinSettings.titlebarDoubleClickCommand
                onActivated: kcm.kwinSettings.titlebarDoubleClickCommand = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "TitlebarDoubleClickCommand"
                }
            }
        }

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "Mouse &wheel:")
            contentItem: QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nd("kcmkwm", "Raise/lower"), value: KWinOptionsSettings.CommandTitlebarWheel_RaiseLower },
                    { text: i18nd("kcmkwm", "Maximize/restore"), value: KWinOptionsSettings.CommandTitlebarWheel_MaxmizeRestore },
                    { text: i18nd("kcmkwm", "Keep above/below"), value: KWinOptionsSettings.CommandTitlebarWheel_AboveBelow },
                    { text: i18nd("kcmkwm", "Move to previous/next desktop"), value: KWinOptionsSettings.CommandTitlebarWheel_PreviousNextDesktop },
                    { text: i18nd("kcmkwm", "Change opacity"), value: KWinOptionsSettings.CommandTitlebarWheel_ChangeOpacity },
                    { text: i18nd("kcmkwm", "Do nothing"), value: KWinOptionsSettings.CommandTitlebarWheel_Nothing },
                ]
                currentValue: kcm.kwinSettings.commandTitlebarWheel
                onActivated: kcm.kwinSettings.commandTitlebarWheel = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "CommandTitlebarWheel"
                }
            }
        }
    }


    Kirigami.FormGroup {
        title: i18nd("kcmkwm", "Titlebar and Frame Actions")

        Kirigami.FormEntry {
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    horizontalAlignment: Qt.AlignHCenter
                    Layout.preferredWidth: titlebarAndFrameLeftFocusedComboBox.implicitWidth
                    text: i18ndc("kcmkwm", "@label for a group of comboboxes Left/Middle/Right-Click for focused windows", "Focused")
                }
                QQC2.Label {
                    horizontalAlignment: Qt.AlignHCenter
                    Layout.preferredWidth: titlebarAndFrameLeftUnfocusedComboBox.implicitWidth
                    text: i18ndc("kcmkwm", "@label for a group of comboboxes Left/Middle/Right-Click for unfocused windows", "Unfocused")
                }
            }
        }

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "&Left click:")
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    id: titlebarAndFrameLeftFocusedComboBox
                    Accessible.name: i18ndc("kcmkwm", "@label:listbox accessible name", "Left click on a focused titlebar or frame")
                    textRole: "text"
                    valueRole: "value"
                    model: root.commandActiveTitlebarModel

                    currentValue: kcm.kwinSettings.commandActiveTitlebar1
                    onActivated: kcm.kwinSettings.commandActiveTitlebar1 = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandActiveTitlebar1"
                    }
                }
                QQC2.ComboBox {
                    id: titlebarAndFrameLeftUnfocusedComboBox
                    Accessible.name: i18ndc("kcmkwm", "@label:listbox accessible name", "Left click on an unfocused titlebar or frame")
                    textRole: "text"
                    valueRole: "value"
                    model: root.commandInactiveTitlebarModel

                    currentValue: kcm.kwinSettings.commandInactiveTitlebar1
                    onActivated: kcm.kwinSettings.commandInactiveTitlebar1 = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandInactiveTitlebar1"
                    }
                }
            }
        }

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "&Middle click:")
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    Accessible.name: i18ndc("kcmkwm", "@label:listbox accessible name", "Middle click on a focused titlebar or frame")
                    textRole: "text"
                    valueRole: "value"
                    model: root.commandActiveTitlebarModel

                    currentValue: kcm.kwinSettings.commandActiveTitlebar2
                    onActivated: kcm.kwinSettings.commandActiveTitlebar2 = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandActiveTitlebar2"
                    }
                }
                QQC2.ComboBox {
                    Accessible.name: i18ndc("kcmkwm", "@label:listbox accessible name", "Middle click on an unfocused titlebar or frame")
                    textRole: "text"
                    valueRole: "value"
                    model: root.commandInactiveTitlebarModel

                    currentValue: kcm.kwinSettings.commandInactiveTitlebar2
                    onActivated: kcm.kwinSettings.commandInactiveTitlebar2 = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandInactiveTitlebar2"
                    }
                }
            }
        }

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "&Right click:")
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    Accessible.name: i18ndc("kcmkwm", "@label:listbox accessible name", "Right click on a focused titlebar or frame")
                    textRole: "text"
                    valueRole: "value"
                    model: root.commandActiveTitlebarModel

                    currentValue: kcm.kwinSettings.commandActiveTitlebar3
                    onActivated: kcm.kwinSettings.commandActiveTitlebar3 = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandActiveTitlebar3"
                    }
                }
                QQC2.ComboBox {
                    Accessible.name: i18ndc("kcmkwm", "@label:listbox accessible name", "Right click on an unfocused titlebar or frame")
                    textRole: "text"
                    valueRole: "value"
                    model: root.commandInactiveTitlebarModel

                    currentValue: kcm.kwinSettings.commandInactiveTitlebar3
                    onActivated: kcm.kwinSettings.commandInactiveTitlebar3 = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandInactiveTitlebar3"
                    }
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.CheckBox {
                text: i18nd("kcmkwm", "Maximize window by double clicking its frame")

                checked: kcm.kwinSettings.doubleClickBorderToMaximize
                onCheckedChanged: kcm.kwinSettings.doubleClickBorderToMaximize = checked

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "DoubleClickBorderToMaximize"
                }
            }
        }
    }


    Kirigami.FormGroup {
        title: i18nd("kcmkwm", "Maximize Button Actions")

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "L&eft click:")
            contentItem: QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: root.maximizeButtonClickCommandModel

                currentValue: kcm.kwinSettings.maximizeButtonLeftClickCommand
                onActivated: kcm.kwinSettings.maximizeButtonLeftClickCommand = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "MaximizeButtonLeftClickCommand"
                }
            }
        }

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "Middle c&lick:")
            contentItem: QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: root.maximizeButtonClickCommandModel

                currentValue: kcm.kwinSettings.maximizeButtonMiddleClickCommand
                onActivated: kcm.kwinSettings.maximizeButtonMiddleClickCommand = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "MaximizeButtonMiddleClickCommand"
                }
            }
        }

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "Right clic&k:")
            contentItem: QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: root.maximizeButtonClickCommandModel

                currentValue: kcm.kwinSettings.maximizeButtonRightClickCommand
                onActivated: kcm.kwinSettings.maximizeButtonRightClickCommand = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "MaximizeButtonRightClickCommand"
                }
            }
        }
    }
}
