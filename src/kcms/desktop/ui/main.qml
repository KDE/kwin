/*
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami 2.20 as Kirigami

KCM.ScrollViewKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 35
    implicitHeight: Kirigami.Units.gridUnit * 30

    actions:  [
        Kirigami.Action {
            displayComponent: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18nc("@text:label Number of rows, label associated to a number input field", "Rows:")
                }
                QQC2.SpinBox {
                    id: rowsSpinBox

                    from: 1
                    to: 20
                    editable: true
                    value: kcm.desktopsModel.rows
                    onValueModified: kcm.desktopsModel.rows = value

                    KCM.SettingHighlighter {
                        highlight: kcm.desktopsModel.rows !== 2
                    }
                    Connections {
                        target: kcm.desktopsModel

                        function onReadyChanged() {
                            rowsSpinBox.value = kcm.desktopsModel.rows;
                        }
                        function onRowsChanged() {
                            rowsSpinBox.value = kcm.desktopsModel.rows;
                        }
                    }
                }
            }
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Add Desktop")
            icon.name: "list-add"
            displayHint: Kirigami.DisplayHint.KeepVisible
            onTriggered: kcm.desktopsModel.createDesktop()
        }
    ]

    Component {
        id: desktopsListItemComponent

        QQC2.ItemDelegate {
            width: ListView.view.width
            down: false  // Disable press effect
            hoverEnabled: false

            // use alternating background colors to visually connect list items'
            // left and right side content elements
            Kirigami.Theme.useAlternateBackgroundColor: true

            contentItem: StackLayout {
                Kirigami.TitleSubtitleWithActions {
                    title: renameLayout.visible ? "" : model ? model.display : ""
                    elide: Text.ElideRight
                    displayHint: QQC2.Button.IconOnly
                    actions: [
                        Kirigami.Action {
                            icon.name: "edit-entry-symbolic"
                            enabled: !renameLayout.visible
                            text: i18nc("@action:button", "Rename")
                            onTriggered: {
                                renameLayout.visible = true;
                                renameField.forceActiveFocus();
                            }
                            tooltip: text
                        },
                        Kirigami.Action {
                            enabled: model && !model.IsMissing && desktopsList.count !== 1 && !renameLayout.visible
                            icon.name: "edit-delete-remove-symbolic"
                            text: i18nc("@info:tooltip", "Remove")
                            onTriggered: {
                                kcm.desktopsModel.removeDesktop(model.Id);
                            }
                        }
                    ]
                }
                RowLayout {
                    id: renameLayout
                    QQC2.TextField {
                        id: renameField
                    }
                    QQC2.Button {
                        id: acceptEditButton
                        icon.name: "dialog-ok-apply"
                        text: i18nc("@info:tooltip", "Apply new name")
                        onClicked: {
                            Qt.callLater(kcm.desktopsModel.setDesktopName, model.Id, renameField.text);
                            renameLayout.visible = false;
                        }
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        display: QQC2.Button.IconOnly
                    }
                    QQC2.Button {
                        id: discardEditButton
                        icon.name: "dialog-cancel-symbolic"
                        text: i18nc("@info:tooltip", "Cancel rename")
                        onClicked: {
                            renameLayout.visible = false;
                        }
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        display: QQC2.Button.IconOnly
                    }

                    onVisibleChanged: {
                        renameField.text = model ? model.display : "";
                        renameField.selectAll();
                    }
                }
            }
        }
    }

    component DelegateButton: QQC2.ToolButton {
        display: QQC2.AbstractButton.IconOnly
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
    }

    header: ColumnLayout {
        id: messagesLayout

        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true

            type: Kirigami.MessageType.Error

            text: kcm.desktopsModel.error

            visible: kcm.desktopsModel.error !== ""
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true

            type: Kirigami.MessageType.Information

            text: i18n("Virtual desktops have been changed outside this settings application. Saving now will overwrite the changes.")

            visible: kcm.desktopsModel.serverModified
        }
    }

    view: ListView {
        id: desktopsList

        clip: true

        model: kcm.desktopsModel.ready ? kcm.desktopsModel : null

        section.property: "DesktopRow"
        section.delegate: Kirigami.ListSectionHeader {
            width: desktopsList.width
            label: i18n("Row %1", section)
        }

        delegate: desktopsListItemComponent
        reuseItems: true
    }

    extraFooterTopPadding: true // re-add separator line
    footer: ColumnLayout {
        Kirigami.FormLayout {

            QQC2.CheckBox {
                id: navWraps

                Kirigami.FormData.label: i18n("Options:")

                text: i18n("Navigation wraps around")
                enabled: !kcm.virtualDesktopsSettings.isImmutable("rollOverDesktops")
                checked: kcm.virtualDesktopsSettings.rollOverDesktops
                onToggled: kcm.virtualDesktopsSettings.rollOverDesktops = checked

                KCM.SettingStateBinding {
                    configObject: kcm.virtualDesktopsSettings
                    settingName: "rollOverDesktops"
                }
            }

            RowLayout {
                Layout.fillWidth: true

                QQC2.CheckBox {
                    id: animationEnabled
                    Layout.fillWidth: true

                    text: i18n("Show animation when switching:")

                    checked: kcm.animationsModel.animationEnabled

                    onToggled: kcm.animationsModel.animationEnabled = checked

                    KCM.SettingHighlighter {
                        highlight: kcm.animationsModel.animationEnabled !== kcm.animationsModel.defaultAnimationEnabled
                    }
                }

                QQC2.ComboBox {
                    enabled: animationEnabled.checked

                    model: kcm.animationsModel
                    textRole: "NameRole"
                    currentIndex: kcm.animationsModel.animationIndex
                    onActivated: kcm.animationsModel.animationIndex = currentIndex

                    KCM.SettingHighlighter {
                        highlight: kcm.animationsModel.animationIndex !== kcm.animationsModel.defaultAnimationIndex
                    }
                }

                QQC2.Button {
                    enabled: animationEnabled.checked && kcm.animationsModel.currentConfigurable

                    icon.name: "configure"

                    onClicked: kcm.configureAnimation()
                }

                QQC2.Button {
                    enabled: animationEnabled.checked

                    icon.name: "dialog-information"

                    onClicked: kcm.showAboutAnimation()
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                Layout.fillWidth: true

                QQC2.CheckBox {
                    id: osdEnabled

                    text: i18n("Show on-screen display when switching:")

                    checked: kcm.virtualDesktopsSettings.desktopChangeOsdEnabled

                    onToggled: kcm.virtualDesktopsSettings.desktopChangeOsdEnabled = checked

                    KCM.SettingStateBinding {
                        configObject: kcm.virtualDesktopsSettings
                        settingName: "desktopChangeOsdEnabled"
                    }
                }

                QQC2.SpinBox {
                    id: osdDuration

                    from: 0
                    to: 10000
                    stepSize: 100

                    textFromValue: (value, locale) => i18n("%1 ms", value)
                    valueFromText: (text, locale) => Number.fromLocaleString(locale, text.split(" ")[0])

                    value: kcm.virtualDesktopsSettings.popupHideDelay

                    onValueModified: kcm.virtualDesktopsSettings.popupHideDelay = value

                    KCM.SettingStateBinding {
                        configObject: kcm.virtualDesktopsSettings
                        settingName: "popupHideDelay"
                        extraEnabledConditions: osdEnabled.checked
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.preferredWidth: Kirigami.Units.gridUnit
                }

                QQC2.CheckBox {
                    id: osdTextOnly
                    text: i18n("Show desktop layout indicators")
                    checked: !kcm.virtualDesktopsSettings.textOnly
                    onToggled: kcm.virtualDesktopsSettings.textOnly = !checked

                    KCM.SettingStateBinding {
                        configObject: kcm.virtualDesktopsSettings
                        settingName: "textOnly"
                        extraEnabledConditions: osdEnabled.checked
                    }
                }
            }
        }
    }
}

