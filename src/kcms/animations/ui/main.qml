/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

import org.kde.plasma.kcm.animations

KCM.SimpleKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 40

    readonly property real animationsWidth: Math.max(Kirigami.Units.gridUnit * 16,
                                                     windowOpenClose.implicitWidth,
                                                     windowMinimize.implicitWidth,
                                                     peekDesktop.implicitWidth,
                                                     virtualDesktop.implicitWidth)

    Kirigami.FormLayout {
        // TODO: Attribute code from plasma-desktop/kcms/workspaceoptions/ui/main.qml:
        // We want to show the slider in a logarithmic way. ie
        // move from 4x, 3x, 2x, 1x, 0.5x, 0.25x, 0.125x
        // 0 is a special case, which means "instant speed / no animations"
        ColumnLayout {
            Kirigami.FormData.label: i18n("Animation speed:")
            Kirigami.FormData.buddyFor: animationSpeedSlider
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 16
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: animationSpeedSlider
                Layout.fillWidth: true
                from: -4
                to: 4
                stepSize: 0.5
                snapMode: QQC2.Slider.SnapAlways
                onMoved: kcm.globalsSettings.animationDurationFactor =
                    (value === to) ? 0 : (1.0 / Math.pow(2, value))
                value: (kcm.globalsSettings.animationDurationFactor === 0)
                    ? animationSpeedSlider.to
                    : -(Math.log(kcm.globalsSettings.animationDurationFactor) / Math.log(2))

                KCM.SettingStateBinding {
                    configObject: kcm.globalsSettings
                    settingName: "animationDurationFactor"
                }
            }

            RowLayout {
                spacing: 0

                QQC2.Label {
                    text: i18nc("Animation speed", "Slow")
                    textFormat: Text.PlainText
                }

                Item {
                    Layout.fillWidth: true
                }

                QQC2.Label {
                    text: i18nc("Animation speed", "Instant")
                    textFormat: Text.PlainText
                }
            }
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        RowLayout {
            id: windowOpenClose
            Kirigami.FormData.label: i18nc("@label:listbox", "Window open/close animation:")
            Layout.preferredWidth: root.animationsWidth

            spacing: Kirigami.Units.smallSpacing

            AnimationComboBox {
                id: windowOpenCloseComboBox
                Layout.fillWidth: true

                animationsModel: kcm.windowOpenCloseAnimations

                KCM.SettingHighlighter {
                    highlight: !windowOpenCloseComboBox.isDefault
                }
            }

            QQC2.Button {
                icon.name: "configure"
                text: i18nc("@info:tooltip", "Configure…")

                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered

                enabled: windowOpenCloseComboBox.isConfigurable
                onClicked: kcm.configure(windowOpenCloseComboBox.configurePluginId, root)
            }
        }

        RowLayout {
            id: windowMinimize
            Kirigami.FormData.label: i18nc("@label:listbox", "Window minimize animation:")
            Layout.preferredWidth: root.animationsWidth

            spacing: Kirigami.Units.smallSpacing

            AnimationComboBox {
                id: windowMinimizeComboBox
                Layout.fillWidth: true

                animationsModel: kcm.windowMinimizeAnimations

                KCM.SettingHighlighter {
                    highlight: !windowMinimizeComboBox.isDefault
                }
            }

            QQC2.Button {
                icon.name: "configure"
                text: i18nc("@info:tooltip", "Configure…")

                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered

                enabled: windowMinimizeComboBox.isConfigurable
                onClicked: kcm.configure(windowMinimizeComboBox.configurePluginId, root)
            }
        }

        RowLayout {
            id: peekDesktop
            Kirigami.FormData.label: i18nc("@label:listbox", "Peek at desktop animation:")
            Layout.preferredWidth: root.animationsWidth

            spacing: Kirigami.Units.smallSpacing

            AnimationComboBox {
                id: peekDesktopComboBox
                Layout.fillWidth: true

                animationsModel: kcm.peekDesktopAnimations

                KCM.SettingHighlighter {
                    highlight: !peekDesktopComboBox.isDefault
                }
            }

            QQC2.Button {
                icon.name: "configure"
                text: i18nc("@info:tooltip", "Configure…")

                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered

                enabled: peekDesktopComboBox.isConfigurable
                onClicked: kcm.configure(peekDesktopComboBox.configurePluginId, root)
            }
        }

        RowLayout {
            id: virtualDesktop
            Kirigami.FormData.label: i18nc("@label:listbox", "Virtual desktop switching animation:")
            Layout.preferredWidth: root.animationsWidth

            spacing: Kirigami.Units.smallSpacing

            AnimationComboBox {
                id: virtualDesktopComboBox
                Layout.fillWidth: true

                animationsModel: kcm.virtualDesktopAnimations

                KCM.SettingHighlighter {
                    highlight: !virtualDesktopComboBox.isDefault
                }
            }

            QQC2.Button {
                icon.name: "configure"
                text: i18nc("@info:tooltip", "Configure…")

                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                enabled: virtualDesktopComboBox.isConfigurable
                onClicked: kcm.configure(virtualDesktopComboBox.configurePluginId, root)
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            visible: effectsKCMButton.visible
        }

        QQC2.Button {
            id: effectsKCMButton
            Kirigami.FormData.label: i18nc("@title:group translate as short as possible", "More effects settings:")
            Layout.preferredWidth: Kirigami.Units.gridUnit * 10

            readonly property var kcmData: kcm.effectsKCMData()

            visible: "icon" in kcmData && "name" in kcmData

            // For consistency with kcm_landingpage
            implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                     implicitContentHeight + topPadding + bottomPadding)

            leftPadding: Kirigami.Units.largeSpacing
            rightPadding: Kirigami.Units.largeSpacing
            topPadding: Kirigami.Units.largeSpacing
            bottomPadding: Kirigami.Units.largeSpacing

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignCenter

                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    source: "icon" in effectsKCMButton.kcmData ? effectsKCMButton.kcmData.icon : ""
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    text: "name" in effectsKCMButton.kcmData ? effectsKCMButton.kcmData.name : ""
                }
            }

            onClicked: kcm.launchEffectsKCM()
        }
    }
}

