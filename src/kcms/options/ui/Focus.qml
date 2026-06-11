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
    Kirigami.FormGroup {
        Kirigami.FormEntry {
            contentItem: QQC2.ComboBox {
                id: focusPolicyComboBox

                Kirigami.FormData.label: i18ndc("kcmkwm", "@label:listbox", "Window focus policy:")
                textRole: "title"
                valueRole: "value"

                delegate: QQC2.ItemDelegate {
                    id: delegate

                    required property string title
                    required property string description
                    required property int index

                    text: delegate.title

                    contentItem: Kirigami.TitleSubtitle {
                        title: delegate.title
                        subtitle: delegate.description
                        font: delegate.font
                        selected: delegate.highlighted || delegate.down
                        wrapMode: Text.Wrap
                    }
                }

                model: [
                    {
                        title: i18ndc("kcmkwm", "@item:inlistbox Window focus policy", "Click to focus"),
                        description: i18ndc("kcmkwm", "@info:tooltip Window focus policy", "Window becomes focused when clicked."),
                        value: KWinOptionsSettings.ClickToFocus
                    },
                    {
                        title: i18ndc("kcmkwm", "@item:inlistbox Window focus policy", "Focus follows pointer"),
                        description: i18ndc("kcmkwm", "@info:tooltip Window focus policy", "As the pointer moves, windows under it become focused after a configurable delay."),
                        value: KWinOptionsSettings.FocusFollowsMouse
                    },
                    {
                        title: i18ndc("kcmkwm", "@item:inlistbox Window focus policy", "Focus under pointer"),
                        description: i18ndc("kcmkwm", "@info:tooltip Window focus policy", "Window under pointer receives focus, even if the pointer has not moved."),
                        value: KWinOptionsSettings.FocusUnderMouse
                    }
                ]
                currentValue: kcm.kwinSettings.focusPolicy
                onActivated: kcm.kwinSettings.focusPolicy = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "FocusPolicy"
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: kcm.kwinSettings.focusPolicy === KWinOptionsSettings.FocusUnderMouse
            type: Kirigami.MessageType.Warning
            text: xi18ndc("kcmkwm", "@info", "Selecting <interface>Focus under pointer</interface> will prevent focus on some windows appearing on screen. This can lead to assigning focus to unwanted windows. Use with caution.")
        }

        Repeater {
            model: [
                {
                    text: i18ndc("kcmkwm", "@option:radio After closing a window, move focus to:", "Last focused window"),
                    value: false,
                    title: i18ndc("kcmkwm", "@title:group", "After closing a window, move focus to:")
                },
                {
                    text: i18ndc("kcmkwm", "@option:radio After closing a window, move focus to:", "Window under pointer"),
                    value: true,
                },
            ]

            delegate: Kirigami.FormEntry {
                id: nextFocusPrefersMouseFormEntry

                required property var modelData
                title: modelData.title || null

                contentItem: QQC2.RadioButton {
                    text: nextFocusPrefersMouseFormEntry.modelData.text
                    checked: kcm.kwinSettings.nextFocusPrefersMouse === nextFocusPrefersMouseFormEntry.modelData.value
                    onToggled: kcm.kwinSettings.nextFocusPrefersMouse = nextFocusPrefersMouseFormEntry.modelData.value

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "NextFocusPrefersMouse"
                        extraEnabledConditions: kcm.kwinSettings.focusPolicy !== KWinOptionsSettings.FocusUnderMouse
                    }
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.SpinBox {
                id: delayFocusSpinBox

                readonly property string label: i18nd("kcmkwm", "&Delay focus by:")
                Kirigami.FormData.label: label
                Accessible.name: label

                from: 0
                to: 3000
                stepSize: 100

                textFromValue: (value, locale) => {
                    return i18ndc("kcmkwm", "short for millisecond(s)", "%1 ms", value)
                }

                valueFromText: (text, locale) => {
                    return Number.fromLocaleString(locale, text.replace(i18ndc("kcmkwm", "short for millisecond(s)", "ms"), ""))
                }

                value: kcm.kwinSettings.delayFocusInterval
                onValueModified: kcm.kwinSettings.delayFocusInterval = value

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "DelayFocusInterval"
                    extraEnabledConditions: kcm.kwinSettings.focusPolicy !== KWinOptionsSettings.ClickToFocus
                }
            }
        }

        Kirigami.FormEntry {
            Kirigami.FormData.buddyFor: focusStealingPreventionComboBox
            contentItem: QQC2.ComboBox {
                id: focusStealingPreventionComboBox
                Kirigami.FormData.label: i18nd("kcmkwm", "Focus &stealing prevention:")
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nd("kcmkwm", "None"), value: 0 },
                    { text: i18nd("kcmkwm", "Low"), value: 1 },
                    { text: i18nd("kcmkwm", "Medium"), value: 2 },
                    { text: i18nd("kcmkwm", "High"), value: 3 },
                    { text: i18nd("kcmkwm", "Extreme"), value: 4 },
                ]

                currentValue: kcm.kwinSettings.focusStealingPreventionLevel
                onActivated: kcm.kwinSettings.focusStealingPreventionLevel = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "FocusStealingPreventionLevel"
                    extraEnabledConditions: kcm.kwinSettings.focusPolicy !== KWinOptionsSettings.FocusUnderMouse
                }
            }
        }

        Repeater {
            model: [
                {
                    text: i18ndc("kcmkwm", "@option:radio When focusing a window on a different Virtual Desktop:", "Switch to window’s Virtual Desktop"),
                    value: KWinOptionsSettings.SwitchToOtherDesktop,
                    title: i18ndc("kcmkwm", "@title:group", "When focusing a window on a different Virtual Desktop:")
                },
                {
                    text: i18nd("kcmkwm", "Bring window to current Virtual Desktop"),
                    value: KWinOptionsSettings.BringToCurrentDesktop,
                },
                {
                    text: i18nd("kcmkwm", "Do nothing"),
                    value: 2
                },
            ]

            delegate: Kirigami.FormEntry {
                id: activationDesktopFormEntry

                required property var modelData
                title: modelData.title || null

                contentItem: QQC2.RadioButton {
                    text: activationDesktopFormEntry.modelData.text
                    checked: kcm.kwinSettings.activationDesktopPolicy === activationDesktopFormEntry.modelData.value
                    onToggled: kcm.kwinSettings.activationDesktopPolicy = activationDesktopFormEntry.modelData.value

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "ActivationDesktopPolicy"
                    }
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.CheckBox {
                id: clickRaise
                Kirigami.FormData.label: i18nd("kcmkwm", "Raising windows:")
                text: i18nd("kcmkwm", "&Click raises focused window")

                checked: kcm.kwinSettings.clickRaise
                onCheckedChanged: kcm.kwinSettings.clickRaise = checked

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "ClickRaise"
                    extraEnabledConditions: !kcm.kwinSettings.autoRaise || kcm.kwinSettings.focusPolicy === KWinOptionsSettings.ClickToFocus
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.FormData.buddyFor: autoRaise
                QQC2.CheckBox {
                    id: autoRaise
                    text: i18nd("kcmkwm", "&Raise on hover, delayed by:")

                    checked: kcm.kwinSettings.autoRaise
                    onCheckedChanged: kcm.kwinSettings.autoRaise = checked

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "AutoRaise"
                        extraEnabledConditions: kcm.kwinSettings.focusPolicy !== KWinOptionsSettings.ClickToFocus
                    }
                }
                QQC2.SpinBox {
                    id: autoRaiseIntervalSpinBox

                    Kirigami.FormData.label: autoRaise.text
                    Accessible.name: autoRaise.text

                    from: 0
                    to: 3000
                    stepSize: 100

                    textFromValue: (value, locale) => {
                        return i18ndc("kcmkwm", "short for millisecond(s)", "%1 ms", value)
                    }

                    valueFromText: (text, locale) => {
                        return Number.fromLocaleString(locale, text.replace(i18ndc("kcmkwm", "short for millisecond(s)", "ms"), ""))
                    }

                    value: kcm.kwinSettings.autoRaiseInterval
                    onValueModified: kcm.kwinSettings.autoRaiseInterval = value

                    KCM.SettingStateBinding {
                        configObject: kcm.kwinSettings
                        settingName: "AutoRaiseInterval"
                        extraEnabledConditions: kcm.kwinSettings.focusPolicy !== KWinOptionsSettings.ClickToFocus
                    }
                }
            }
        }

        Kirigami.FormEntry {
            visible: Qt.application.screens.length > 1
            contentItem: QQC2.CheckBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "Multiscreen behavior:")
                text: i18nd("kcmkwm", "&Separate screen focus")

                checked: kcm.kwinSettings.separateScreenFocus
                onCheckedChanged: kcm.kwinSettings.separateScreenFocus = checked

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "SeparateScreenFocus"
                }
            }
        }
    }
}
