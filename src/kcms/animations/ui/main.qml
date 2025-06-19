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

    ColumnLayout {
        spacing: 0

        // NOTE: FormLayouts are separate due to usage of repeaters, which
        // otherwise place their delegates at the end

        Kirigami.FormLayout {
            id: mainLayout

            twinFormLayouts: separatorLayout

            // We want to show the slider in a logarithmic way. ie
            // move from 4x, 3x, 2x, 1x, 0.5x, 0.25x, 0.125x
            // 0 is a special case, which means "instant speed / no animations"
            GridLayout {
                Kirigami.FormData.labelAlignment: Qt.AlignTop
                Kirigami.FormData.label: i18n("Global animation speed:")
                Kirigami.FormData.buddyFor: animationSpeedSlider
                Layout.fillWidth: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 16

                rowSpacing: Kirigami.Units.smallSpacing
                columnSpacing: Kirigami.Units.smallSpacing

                columns: 2

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

                Kirigami.ContextualHelpButton {
                    Layout.alignment: Qt.AlignTop

                    toolTipText: xi18nc("@info:tooltip", "Some applications do not support this setting: In particular, GTK applications cannot change animation duration, but will still disable animations when <interface>animation speed</interface> is <interface>Instant</interface>.")
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
                Kirigami.FormData.label: i18nc("@title:group", "Desktop animations")
            }

            Repeater {
                model: [{"animationsModel": kcm.windowOpenCloseAnimations,  "label": i18nc("@label:listbox", "Window open/close:")        },
                        {"animationsModel": kcm.windowMaximizeAnimations,   "label": i18nc("@label:listbox", "Window maximize:")          },
                        {"animationsModel": kcm.windowMinimizeAnimations,   "label": i18nc("@label:listbox", "Window minimize:")          },
                        {"animationsModel": kcm.windowFullscreenAnimations, "label": i18nc("@label:listbox", "Window full screen:")       },
                        {"animationsModel": kcm.peekDesktopAnimations,      "label": i18nc("@label:listbox", "Peek at desktop:")          },
                        {"animationsModel": kcm.virtualDesktopAnimations,   "label": i18nc("@label:listbox", "Virtual desktop switching:")}]
                delegate: RowLayout {
                    id: animationLayout
                    Kirigami.FormData.buddyFor: animationComboBox
                    Kirigami.FormData.label: modelData.label
                    Layout.fillWidth: true
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 16

                    spacing: Kirigami.Units.smallSpacing

                    enabled: kcm.globalsSettings.animationDurationFactor != 0

                    AnimationComboBox {
                        id: animationComboBox
                        Layout.fillWidth: true
                        // Reserve space for the hidden configure button
                        Layout.rightMargin: !animationConfigure.visible ? (animationConfigure.implicitWidth + animationLayout.spacing) : undefined

                        animationsModel: modelData.animationsModel

                        KCM.SettingHighlighter {
                            highlight: !animationComboBox.isDefault && kcm.globalsSettings.animationDurationFactor != 0
                        }
                    }

                    QQC2.Button {
                        id: animationConfigure
                        icon.name: "configure"
                        text: i18nc("@info:tooltip", "Configure…")
                        display: QQC2.AbstractButton.IconOnly

                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        enabled: animationComboBox.isConfigurable
                        visible: animationComboBox.isAnyConfigurable
                        onClicked: kcm.configure(animationComboBox.configurePluginId, root)
                    }
                }
            }

            Connections {
                target: kcm.otherEffects
                function onModelReset() { otherEffectsRepeater.model = kcm.otherEffects.rowCount(); }
            }

            Repeater {
                id: otherEffectsRepeater

                model: kcm.otherEffects.rowCount()
                delegate: RowLayout {
                    id: otherAnimationLayout
                    Kirigami.FormData.buddyFor: animationCheckBox
                    Kirigami.FormData.label: i18nc("@option:check %1 is the name of an animation, e.g. 'Login' or 'Logout'",
                                                   "%1:",
                                                   kcm.otherEffects.data(kcm.otherEffects.index(index, 0), EffectsModel.NameRole))
                    Layout.fillWidth: true

                    required property int index

                    spacing: Kirigami.Units.smallSpacing

                    enabled: kcm.globalsSettings.animationDurationFactor != 0

                    AnimationCheckBox {
                        id: animationCheckBox
                        Layout.fillWidth: true
                        // Reserve space for the hidden configure button
                        Layout.rightMargin: !otherAnimationConfigure.visible ? (otherAnimationConfigure.implicitWidth + otherAnimationLayout.spacing) : undefined

                        implicitWidth: 0

                        animationsModel: kcm.otherEffects
                        index: otherAnimationLayout.index

                        text: kcm.otherEffects.data(kcm.otherEffects.index(index, 0), EffectsModel.DescriptionRole)

                        // Center the indicator when multi-line
                        Binding {
                            target: animationCheckBox.indicator.anchors
                            property: "verticalCenter"
                            value: animationCheckBox.verticalCenter
                            when: animationCheckBox.contentItem.lineCount > 1
                        }


                        KCM.SettingHighlighter {
                            highlight: !animationCheckBox.isDefault && kcm.globalsSettings.animationDurationFactor != 0
                        }
                    }

                    QQC2.Button {
                        id: otherAnimationConfigure
                        icon.name: "configure"
                        text: i18nc("@info:tooltip", "Configure…")
                        display: QQC2.AbstractButton.IconOnly

                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        enabled: animationCheckBox.checked
                        visible: animationCheckBox.isConfigurable
                        onClicked: kcm.configure(animationCheckBox.configurePluginId, root)
                    }
                }
            }
        }

        Kirigami.FormLayout {
            id: separatorLayout

            twinFormLayouts: mainLayout

            Kirigami.Separator {
                id: separator
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
}
