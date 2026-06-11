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

    readonly property var commandWindow23Model: [
        { text: i18nd("kcmkwm", "Focus, raise, and click through"), value: KWinOptionsSettings.CommandWindow2_ActivateRaisePassClick },
        { text: i18nd("kcmkwm", "Focus and click through"), value: KWinOptionsSettings.CommandWindow2_ActivatePassClick },
        { text: i18nd("kcmkwm", "Focus"), value: KWinOptionsSettings.CommandWindow2_Activate },
        { text: i18nd("kcmkwm", "Focus and raise"), value: KWinOptionsSettings.CommandWindow2_ActivateRaise }
    ]

    readonly property var commandAllModel: [
        { text: i18nd("kcmkwm", "Move"), value: KWinOptionsSettings.CommandAll_Move },
        { text: i18nd("kcmkwm", "Focus, raise, and move"), value: KWinOptionsSettings.CommandAll_ActivateRaiseAndMove },
        { text: i18nd("kcmkwm", "Focus, raise, and resize"), value: KWinOptionsSettings.CommandAll_ActivateRaiseAndResize },
        { text: i18nd("kcmkwm", "Resize"), value: KWinOptionsSettings.CommandAll_Resize },
        { text: i18nd("kcmkwm", "Raise"), value: KWinOptionsSettings.CommandAll_Raise },
        { text: i18nd("kcmkwm", "Lower"), value: KWinOptionsSettings.CommandAll_Lower },
        { text: i18nd("kcmkwm", "Raise/lower"), value: KWinOptionsSettings.CommandAll_ToggleRaiseAndLower },
        { text: i18nd("kcmkwm", "Minimize"), value: KWinOptionsSettings.CommandAll_Minimize },
        { text: i18nd("kcmkwm", "Decrease opacity"), value: KWinOptionsSettings.CommandAll_DecreaseOpacity },
        { text: i18nd("kcmkwm", "Increase opacity"), value: KWinOptionsSettings.CommandAll_IncreaseOpactiy },
        { text: i18nd("kcmkwm", "Window menu"), value: KWinOptionsSettings.CommandAll_OperationsMenu },
        { text: i18nd("kcmkwm", "Do nothing"), value: KWinOptionsSettings.CommandAll_Nothing },
    ]

    Kirigami.FormGroup {
        title: i18ndc("kcmkwm", "@title:group", "Unfocused Inner Window Actions")

        Kirigami.FormEntry {
            contentItem: QQC2.ComboBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "&Left click:")
                textRole: "text"
                valueRole: "value"
                model: [
                    {text: i18nd("kcmkwm", "Focus, click through, and raise on release"), value: KWinOptionsSettings.CommandWindow1_ActivateRaiseOnReleaseAndPassClick},
                    {text: i18nd("kcmkwm", "Focus, raise, and click through"), value: KWinOptionsSettings.CommandWindow1_ActivateRaisePassClick},
                    {text: i18nd("kcmkwm", "Focus and click through"), value: KWinOptionsSettings.CommandWindow1_ActivatePassClick},
                    {text: i18nd("kcmkwm", "Focus"), value: KWinOptionsSettings.CommandWindow1_Activate},
                    {text: i18nd("kcmkwm", "Focus and raise"), value: KWinOptionsSettings.CommandWindow1_ActivateRaise}
                ]

                currentValue: kcm.kwinSettings.commandWindow1
                onActivated: kcm.kwinSettings.commandWindow1 = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "CommandWindow1"
                }
            }
        }
        Kirigami.FormEntry {
            contentItem: QQC2.ComboBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "&Middle click:")
                textRole: "text"
                valueRole: "value"
                model: root.commandWindow23Model
                currentValue: kcm.kwinSettings.commandWindow2
                onActivated: kcm.kwinSettings.commandWindow2 = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "CommandWindow2"
                }
            }
        }
        Kirigami.FormEntry {
            contentItem: QQC2.ComboBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "&Right click:")
                textRole: "text"
                valueRole: "value"
                model: root.commandWindow23Model

                currentValue: kcm.kwinSettings.commandWindow3
                onActivated: kcm.kwinSettings.commandWindow3 = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "CommandWindow3"
                }
            }
        }
        Kirigami.FormEntry {
            contentItem: QQC2.ComboBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "Mouse &wheel:")
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nd("kcmkwm", "Scroll"), value: KWinOptionsSettings.Scroll },
                    { text: i18nd("kcmkwm", "Focus and scroll"), value: KWinOptionsSettings.ActivateScroll },
                    { text: i18nd("kcmkwm", "Focus, raise, and scroll"), value: KWinOptionsSettings.ActivateRaiseAndScroll },
                ]

                currentValue: kcm.kwinSettings.commandWindowWheel
                onActivated: kcm.kwinSettings.commandWindowWheel = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "CommandWindowWheel"
                }
            }
        }
    }

    Kirigami.FormGroup {
        title: i18nd("kcmkwm", "Inner Window, Titlebar, and Frame Actions")

        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "Mo&difier key:")
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: i18nd("kcmkwm", "Meta"), value: KWinOptionsSettings.Meta },
                        { text: i18nd("kcmkwm", "Alt"), value: KWinOptionsSettings.Alt }
                    ]

                    currentValue: kcm.kwinSettings.commandAllKey
                    onActivated: kcm.kwinSettings.commandAllKey = currentValue

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "CommandAllKey"
                    }
                }

                QQC2.Label {
                    text: "+"
                    horizontalAlignment: Qt.AlignHCenter
                    Layout.minimumWidth: Kirigami.Units.gridUnit
                }

                Kirigami.FormLayout {


                    QQC2.ComboBox {
                        Kirigami.FormData.label: i18nd("kcmkwm", "L&eft click:")
                        textRole: "text"
                        valueRole: "value"
                        model: root.commandAllModel

                        currentValue: kcm.kwinSettings.commandAll1
                        onActivated: kcm.kwinSettings.commandAll1 = currentValue

                        KCM.SettingStateBinding {
                            configObject: kcm.kwinSettings
                            settingName: "CommandAll1"
                        }
                    }


                    QQC2.ComboBox {
                        Kirigami.FormData.label: i18nd("kcmkwm", "Middle &click:")
                        textRole: "text"
                        valueRole: "value"
                        model: root.commandAllModel

                        currentValue: kcm.kwinSettings.commandAll2
                        onActivated: kcm.kwinSettings.commandAll2 = currentValue

                        KCM.SettingStateBinding {
                            configObject: kcm.kwinSettings
                            settingName: "CommandAll2"
                        }
                    }


                    QQC2.ComboBox {
                        Kirigami.FormData.label: i18nd("kcmkwm", "Right clic&k:")
                        textRole: "text"
                        valueRole: "value"
                        model: root.commandAllModel

                        currentValue: kcm.kwinSettings.commandAll3
                        onActivated: kcm.kwinSettings.commandAll3 = currentValue

                        KCM.SettingStateBinding {
                            configObject: kcm.kwinSettings
                            settingName: "CommandAll3"
                        }
                    }


                    QQC2.ComboBox {
                        Kirigami.FormData.label: i18nd("kcmkwm", "Mo&use wheel:")
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18nd("kcmkwm", "Raise/lower"), value: KWinOptionsSettings.CommandAllWheel_RaiseLower },
                            { text: i18nd("kcmkwm", "Maximize/restore"), value: KWinOptionsSettings.CommandAllWheel_MaxmizeRestore },
                            { text: i18nd("kcmkwm", "Keep above/below"), value: KWinOptionsSettings.CommandAllWheel_AboveBelow },
                            { text: i18nd("kcmkwm", "Move to previous/next desktop"), value: KWinOptionsSettings.CommandAllWheel_PreviousNextDesktop },
                            { text: i18nd("kcmkwm", "Change opacity"), value: KWinOptionsSettings.CommandAllWheel_ChangeOpacity },
                            { text: i18nd("kcmkwm", "Do nothing"), value: KWinOptionsSettings.CommandAllWheel_Nothing },
                        ]
                        currentValue: kcm.kwinSettings.commandAllWheel
                        onActivated: kcm.kwinSettings.commandAllWheel = currentValue

                        KCM.SettingStateBinding {
                            configObject: kcm.kwinSettings
                            settingName: "CommandAllWheel"
                        }
                    }
                }
            }
        }
    }
}
