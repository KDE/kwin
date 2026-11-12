/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls

import org.kde.plasma.kcm.kwinoptions

Kirigami.Form {
    Kirigami.FormGroup {
        Kirigami.FormEntry {
            subtitle: xi18nc("@info", "Hold <shortcut>Shift</shortcut> while moving a window to custom tile it.")
            contentItem: QQC2.CheckBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "&Split tiles:")
                text: i18nd("kcmkwm", "When moving a window on a tiled screen")

                checked: kcm.kwinSettings.interactiveTiling
                onCheckedChanged: kcm.kwinSettings.interactiveTiling = checked

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "InteractiveTiling"
                }
            }
        }

        Kirigami.FormEntry {
            subtitle: xi18nc("@info", "This restores old quick tiling behavior. Note that the long term goal is to unify tiling systems, this option will be eventually removed.")
            contentItem: QQC2.CheckBox {
                Kirigami.FormData.label: i18nd("kcmkwm", "Screen &edges:")
                text: i18nd("kcmkwm", "Drag windows to screen edges to custom tile them")

                checked: kcm.kwinSettings.electricBorderCustomTiling
                onCheckedChanged: kcm.kwinSettings.electricBorderCustomTiling = checked

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "ElectricBorderCustomTiling"
                }
            }
        }
    }

    Kirigami.FormGroup {
        Kirigami.FormEntry {
            contentItem: QQC2.RadioButton {
                Kirigami.FormData.label: i18nd("kcmkwm", "Auto-tiling:")
                text: i18nc("@radio part of a complete sentence: 'Auto-tiling: off'", "Off")
                checked: kcm.kwinSettings.autoTiling === KWinOptionsSettings.AutoTiling.Off
                onToggled: kcm.kwinSettings.autoTiling = KWinOptionsSettings.AutoTiling.Off
                QQC2.ButtonGroup { id: autoTilingGroup }
                QQC2.ButtonGroup.group: autoTilingGroup

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "AutoTiling"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: QQC2.RadioButton {
                text: i18nc("@radio part of a complete sentence: 'Auto-tiling: on'", "On")
                checked: kcm.kwinSettings.autoTiling === KWinOptionsSettings.AutoTiling.On
                onToggled: kcm.kwinSettings.autoTiling = KWinOptionsSettings.AutoTiling.On
                QQC2.ButtonGroup.group: autoTilingGroup

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "AutoTiling"
                }
            }
        }

        Kirigami.FormEntry {
            subtitle: xi18nc("@info", "New windows will be auto-tiled only if they are shown on an output that already contains other tiled windows.")
            contentItem: QQC2.RadioButton {
                text: i18nc("@radio part of a complete sentence: 'Auto-tiling: on'", "When screen already contains a tiled window")
                checked: kcm.kwinSettings.autoTiling === KWinOptionsSettings.AutoTiling.OnDemand
                onToggled: kcm.kwinSettings.autoTiling = KWinOptionsSettings.AutoTiling.OnDemand
                QQC2.ButtonGroup.group: autoTilingGroup

                KCM.SettingStateBinding {
                    configObject: kcm.kwinSettings
                    settingName: "AutoTiling"
                }
            }
        }

        Kirigami.FormEntry {
            contentItem: KQuickControls.KeySequenceItem {
                Kirigami.FormData.label: i18ndc("kcmkwm", "A shortcut to until the active screen", "Untile active screen:")
                keySequence: kcm.untileActiveScreenShortcut
                onCaptureFinished: kcm.untileActiveScreenShortcut = keySequence
            }
        }
    }
}
