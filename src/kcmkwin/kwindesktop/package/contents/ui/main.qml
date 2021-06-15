/*
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick 2.5
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Layouts 1.1

import org.kde.kcm 1.5 as KCM
import org.kde.kirigami 2.10 as Kirigami

KCM.ScrollViewKCM {
    id: root

    KCM.ConfigModule.quickHelp: i18n("This module lets you configure the navigation, number and layout of virtual desktops.")

    Connections {
        target: kcm.desktopsModel

        function onReadyChanged() {
            rowsSpinBox.value = kcm.desktopsModel.rows;
        }

        function onRowsChanged() {
            rowsSpinBox.value = kcm.desktopsModel.rows;
        }
    }
    implicitWidth: Kirigami.Units.gridUnit * 35
    implicitHeight: Kirigami.Units.gridUnit * 30

    Component {
        id: desktopsListItemComponent

        Kirigami.SwipeListItem {
            id: listItem

            contentItem: RowLayout {
                QQC2.TextField {
                    id: nameField

                    background: null
                    leftPadding: Kirigami.Units.largeSpacing
                    topPadding: 0
                    bottomPadding: 0

                    Layout.fillWidth: true

                    Layout.alignment: Qt.AlignVCenter

                    text: model ? model.display : ""

                    readOnly: true

                    onTextEdited: {
                        Qt.callLater(kcm.desktopsModel.setDesktopName, model.Id, text);
                    }

                    onEditingFinished: {
                        readOnly = true;
                    }
                }

                Rectangle {
                    id: defaultIndicator
                    radius: width * 0.5
                    implicitWidth: Kirigami.Units.largeSpacing
                    implicitHeight: Kirigami.Units.largeSpacing
                    visible: kcm.defaultsIndicatorsVisible
                    opacity: model ? !model.IsDefault : 0.0
                    color: Kirigami.Theme.neutralTextColor
                }
            }

        actions: [
            Kirigami.Action {
                enabled: model && !model.IsMissing
                iconName: "edit-rename"
                tooltip: i18nc("@info:tooltip", "Rename")
                onTriggered: {
                    nameField.readOnly = false;
                    nameField.selectAll();
                    nameField.forceActiveFocus();
                }
            },
            Kirigami.Action {
                enabled: model && !model.IsMissing && desktopsList.count !== 1
                iconName: "edit-delete"
                tooltip: i18nc("@info:tooltip", "Remove")
                onTriggered: kcm.desktopsModel.removeDesktop(model.Id)
            }]
        }
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

        delegate: Kirigami.DelegateRecycler {
            width: desktopsList.width

            sourceComponent: desktopsListItemComponent
        }
    }

    footer: ColumnLayout {
        RowLayout {
            QQC2.Button {
                text: i18nc("@action:button", "Add")
                icon.name: "list-add"

                onClicked: kcm.desktopsModel.createDesktop(i18n("New Desktop"))
            }

            Item { // Spacer
                Layout.fillWidth: true
            }

            QQC2.SpinBox {
                id: rowsSpinBox

                from: 1
                to: 20
                editable: true
                value: kcm.desktopsModel.rows

                textFromValue: function(value, locale) { return i18np("1 Row", "%1 Rows", value)}
                valueFromText: function(text, locale) { return parseInt(text, 10); }

                onValueModified: kcm.desktopsModel.rows = value

                KCM.SettingHighlighter {
                    highlight: kcm.desktopsModel.rows !== 2
                }
            }
        }

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

                    textFromValue: function(value, locale) { return i18n("%1 ms", value)}
                    valueFromText: function(text, locale) {return Number.fromLocaleString(locale, text.split(" ")[0])}

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
                    width: units.largeSpacing
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

