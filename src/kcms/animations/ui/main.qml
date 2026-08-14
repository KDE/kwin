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
    Kirigami.SizeGroup {
        id: contentItemSizeGroup
        mode: Kirigami.SizeGroup.Width
        items: [
            animationSpeedSlider
        ]
        function appendItem(item: Item) {
            contentItemSizeGroup.items.push(item);
        }
        function removeItem(item: Item) {
            contentItemSizeGroup.items = contentItemSizeGroup.items.filter((e) => e !== item)
        }
    }
    Kirigami.Form {
        Kirigami.FormGroup {
            // We want to show the slider in a logarithmic way. ie
            // move from 4x, 3x, 2x, 1x, 0.5x, 0.25x, 0.125x
            // 0 is a special case, which means "instant speed / no animations"
            Kirigami.FormEntry {
                title: i18n("Global animation speed:")
                contentItem: ColumnLayout {
                    Kirigami.FormData.buddyFor: animationSpeedSlider
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Slider {
                        id: animationSpeedSlider
                        // Use same values as plasma-desktop/kcms/landingpage/ui/main.qml
                        property var valueMapping: [
                            4,
                            2,
                            1.5,
                            1,
                            0.75,
                            0.5,
                            0,
                        ]
                        from: 0
                        to: valueMapping.length - 1
                        stepSize: 1
                        Kirigami.StyleHints.tickMarkStepSize: stepSize
                        snapMode: QQC2.Slider.SnapAlways
                        onMoved: kcm.globalsSettings.animationDurationFactor = valueMapping[value]
                        value: {
                            let factor = kcm.globalsSettings.animationDurationFactor
                            let index = valueMapping.findIndex(item => item <= factor)
                            return index >= 0 ? index : valueMapping.length - 1
                        }
                        // vertically center align slider with help button
                        Layout.topMargin: Math.max(0, (animationSpeedHelpButton.height - height) / 2)
                        KCM.SettingStateBinding {
                            configObject: kcm.globalsSettings
                            settingName: "animationDurationFactor"
                        }
                    }
                    RowLayout {
                        id: animationSpeedLegend
                        Layout.maximumWidth: animationSpeedSlider.width
                        Layout.fillWidth: true
                        spacing: 0
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: i18nc("Animation speed", "Slow")
                            textFormat: Text.PlainText
                            horizontalAlignment: Text.AlignLeft
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: i18nc("Animation speed", "Instant")
                            textFormat: Text.PlainText
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
                trailingItems: [
                    Kirigami.ContextualHelpButton {
                        id: animationSpeedHelpButton
                        Layout.alignment: Qt.AlignTop
                        toolTipText: xi18nc("@info:tooltip", "Some applications do not support this setting: In particular, GTK applications cannot change animation duration, but will still disable animations when <interface>animation speed</interface> is <interface>Instant</interface>.")
                    }
                ]
            }
        }

        Kirigami.FormGroup {
            title: i18nc("@title:group", "Desktop animations")
            Repeater {
                model: [{"animationsModel": kcm.windowOpenCloseAnimations,  "label": i18nc("@label:listbox", "Window open/close:")        },
                        {"animationsModel": kcm.windowMaximizeAnimations,   "label": i18nc("@label:listbox", "Window maximize:")          },
                        {"animationsModel": kcm.windowMinimizeAnimations,   "label": i18nc("@label:listbox", "Window minimize:")          },
                        {"animationsModel": kcm.windowFullscreenAnimations, "label": i18nc("@label:listbox", "Window full screen:")       },
                        {"animationsModel": kcm.peekDesktopAnimations,      "label": i18nc("@label:listbox", "Peek at desktop:")          },
                        {"animationsModel": kcm.virtualDesktopAnimations,   "label": i18nc("@label:listbox", "Virtual desktop switching:")}]
                delegate: Kirigami.FormEntry {
                    id: delegate
                    required property var modelData
                    title: modelData.label
                    enabled: kcm.globalsSettings.animationDurationFactor != 0
                    contentItem: AnimationComboBox {
                        id: animationComboBox
                        animationsModel: delegate.modelData.animationsModel
                        KCM.SettingHighlighter {
                            highlight: !animationComboBox.isDefault && kcm.globalsSettings.animationDurationFactor != 0
                        }
                        Component.onCompleted: contentItemSizeGroup.appendItem(this);
                        Component.onDestruction: contentItemSizeGroup.removeItem(this);
                    }
                    trailingItems: [
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
                    ]
                }
            }

            Connections {
                target: kcm.otherEffects
                function onModelReset() { otherEffectsRepeater.model = kcm.otherEffects.rowCount(); }
            }

            Repeater {
                id: otherEffectsRepeater
                model: kcm.otherEffects.rowCount()
                delegate: Kirigami.FormEntry {
                    id: delegate
                    required property int index
                    title: i18nc("@option:check %1 is the name of an animation, e.g. 'Login' or 'Logout'",
                                                    "%1:",
                                                    kcm.otherEffects.data(kcm.otherEffects.index(index, 0), EffectsModel.NameRole))
                    enabled: kcm.globalsSettings.animationDurationFactor != 0
                    contentItem: AnimationCheckBox {
                        id: animationCheckBox
                        animationsModel: kcm.otherEffects
                        index: delegate.index
                        text: kcm.otherEffects.data(kcm.otherEffects.index(index, 0), EffectsModel.DescriptionRole)
                        KCM.SettingHighlighter {
                            highlight: !animationCheckBox.isDefault && kcm.globalsSettings.animationDurationFactor != 0
                        }
                        Component.onCompleted: contentItemSizeGroup.appendItem(this);
                        Component.onDestruction: contentItemSizeGroup.removeItem(this);
                    }
                    trailingItems: [
                        QQC2.Button {
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
                    ]
                }
            }
        }

        Kirigami.FormGroup {
            id: effectsKCMGroup
            readonly property var kcmData: kcm.effectsKCMData()
            visible: "icon" in kcmData && "name" in kcmData
            Kirigami.FormAction {
                title: i18nc("@title:group translate as short as possible", "More effects settings:")
                action: QQC2.Action {
                    icon.name: effectsKCMGroup.kcmData.icon ?? ""
                    text: effectsKCMGroup.kcmData.name ?? ""
                    onTriggered: kcm.launchEffectsKCM()
                }
            }
        }
    }
}
