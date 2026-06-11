/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

import org.kde.plasma.kcm.kwinoptions

Kirigami.Form {
    Kirigami.FormGroup {
        title: i18ndc("kcmkwm", "@title:group", "Movement")
        Kirigami.FormEntry {
            contentItem: QQC2.SpinBox {
                id: borderSnapZoneSpinBox

                readonly property string label: i18nd("kcmkwm", "Screen &edge snap zone:")
                Kirigami.FormData.label: label
                Accessible.name: label

                from: 0
                to: 100

                textFromValue: (value, locale) => {
                    return value === 0 ? i18nd("kcmkwm", "None") : i18ndc("kcmkwm", "short for pixel(s)", "%1 px", value)
                }

                valueFromText: (text, locale) => {
                    return Number.fromLocaleString(locale, text.replace(i18ndc("kcmkwm", "short for pixel(s)", "px"), ""))
                }

                value: kcm.kwinSettings.borderSnapZone
                onValueModified: kcm.kwinSettings.borderSnapZone = value

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "BorderSnapZone"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.SpinBox {
                id: windowSnapZoneSpinBox
                readonly property string label: i18nd("kcmkwm", "&Window snap zone:")
                Kirigami.FormData.label: label
                Accessible.name: label

                from: 0
                to: 100

                textFromValue: (value, locale) => {
                    return value === 0 ? i18nd("kcmkwm", "None") : i18ndc("kcmkwm", "short for pixel(s)", "%1 px", value)
                }

                valueFromText: (text, locale) => {
                    return Number.fromLocaleString(locale, text.replace(i18ndc("kcmkwm", "short for pixel(s)", "px"), ""))
                }

                value: kcm.kwinSettings.windowSnapZone
                onValueModified: kcm.kwinSettings.windowSnapZone = value

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "WindowSnapZone"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.SpinBox {
                id: centerSnapZoneSpinBox

                readonly property string label: i18nd("kcmkwm", "&Center snap zone:")
                Kirigami.FormData.label: label
                Accessible.name: label

                from: 0
                to: 100

                textFromValue: (value, locale) => {
                    return value === 0 ? i18nd("kcmkwm", "None") : i18ndc("kcmkwm", "short for pixel(s)", "%1 px", value)
                }

                valueFromText: (text, locale) => {
                    return Number.fromLocaleString(locale, text.replace(i18ndc("kcmkwm", "short for pixel(s)", "px"), ""))
                }

                value: kcm.kwinSettings.centerSnapZone
                onValueModified: kcm.kwinSettings.centerSnapZone = value

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "CenterSnapZone"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.CheckBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "&Snap windows:")
                text: i18nd("kcmkwm", "Only when overlapping")

                checked: kcm.kwinSettings.snapOnlyWhenOverlapping
                onCheckedChanged: kcm.kwinSettings.snapOnlyWhenOverlapping = checked

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "SnapOnlyWhenOverlapping"
                }
            }
        }
    }


    Kirigami.FormGroup {
        title: i18ndc("kcmkwm", "@title:group", "Placement")
        Kirigami.FormEntry {
            contentItem: QQC2.ComboBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "Window &placement:")
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nd("kcmkwm", "Minimal Overlapping"), value: KWinOptionsSettings.Smart },
                    { text: i18nd("kcmkwm", "Maximized"), value: KWinOptionsSettings.Maximizing },
                    { text: i18nd("kcmkwm", "Random"), value: KWinOptionsSettings.Random },
                    { text: i18nd("kcmkwm", "Centered"), value: KWinOptionsSettings.Centered },
                    { text: i18nd("kcmkwm", "In Top-Left Corner"), value: KWinOptionsSettings.ZeroCornered },
                    { text: i18nd("kcmkwm", "Under mouse"), value: KWinOptionsSettings.UnderMouse },
                ]
                currentValue: kcm.kwinSettings.placement
                onActivated: kcm.kwinSettings.placement = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "Placement"
                }
            }
        }
    }


    Kirigami.FormGroup {
        title: i18ndc("kcmkwm", "@title:group", "Picture-in-picture Placement")
        visible: kcm.isPiPEnabled
        Kirigami.FormEntry {
            title: i18nd("kcmkwm", "Open in screen corner:")
            contentItem: QQC2.ComboBox {
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nd("kcmkwm", "Top-left"), value: Qt.Corner.TopLeftCorner },
                    { text: i18nd("kcmkwm", "Top-right"), value: Qt.Corner.TopRightCorner },
                    { text: i18nd("kcmkwm", "Bottom-left"), value: Qt.Corner.BottomLeftCorner },
                    { text: i18nd("kcmkwm", "Bottom-right"), value: Qt.Corner.BottomRightCorner },
                ]
                currentValue: kcm.kwinSettings.pictureInPictureHomeCorner
                onActivated: kcm.kwinSettings.pictureInPictureHomeCorner = currentValue

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "PictureInPictureHomeCorner"
                }
            }
        }
        Kirigami.FormEntry {
            id: marginSpinboxLabel
            title: i18nd("kcmkwm", "Margin:")
            contentItem: QQC2.SpinBox {
                id: marginSpinbox

                Accessible.name: marginSpinboxLabel.title
                from: 0
                to: 99
                stepSize: 1

                textFromValue: (value, locale) => {
                    return i18ndc("kcmkwm", "short for pixel(s)", "%1 px", value)
                }

                valueFromText: (text, locale) => {
                    return Number.fromLocaleString(locale, text.replace(i18ndc("kcmkwm", "short for pixel(s)", "px"), ""))
                }

                value: kcm.kwinSettings.pictureInPictureMargin
                onValueModified: kcm.kwinSettings.pictureInPictureMargin = value

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "PictureInPictureMargin"
                }
            }
        }
    }
}
