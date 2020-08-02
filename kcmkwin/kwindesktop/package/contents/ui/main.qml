/*
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick 2.5
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Layouts 1.1

import org.kde.kcm 1.2
import org.kde.kirigami 2.10 as Kirigami
import org.kde.plasma.core 2.1 as PlasmaCore

ScrollViewKCM {
    id: root

    ConfigModule.quickHelp: i18n("This module lets you configure the navigation, number and layout of virtual desktops.")

    Connections {
        target: kcm.desktopsModel

        onReadyChanged: {
            rowsSpinBox.value = kcm.desktopsModel.rows;
        }

        onRowsChanged: {
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

                    text: model.display

                    readOnly: true

                    onEditingFinished: {
                        readOnly = true;
                        Qt.callLater(kcm.desktopsModel.setDesktopName, model.Id, text);
                    }
                }
            }

        actions: [
            Kirigami.Action {
                enabled: !model.IsMissing
                iconName: "edit-rename"
                tooltip: i18nc("@info:tooltip", "Rename")
                onTriggered: {
                    nameField.readOnly = false;
                    nameField.selectAll();
                    nameField.forceActiveFocus();
                }
            },
            Kirigami.Action {
                enabled: !model.IsMissing
                iconName: "edit-delete-remove"
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

            visible: kcm.desktopsModel.error != ""
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

                textFromValue: function(value, locale) { return i18np("1 Row", "%1 Rows", value)}
                valueFromText: function(text, locale) { return parseInt(text, 10); }

                onValueModified: kcm.desktopsModel.rows = value
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
            }

            RowLayout {
                Layout.fillWidth: true

                QQC2.CheckBox {
                    id: animationEnabled
                    // Let it elide but don't make it push the ComboBox away from it
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth

                    text: i18n("Show animation when switching:")

                    checked: kcm.animationsModel.enabled

                    onToggled: kcm.animationsModel.enabled = checked
                }

                QQC2.ComboBox {
                    enabled: animationEnabled.checked

                    model: kcm.animationsModel
                    textRole: "NameRole"
                    currentIndex: kcm.animationsModel.currentIndex
                    onActivated: kcm.animationsModel.currentIndex = currentIndex
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

                    enabled: !kcm.virtualDesktopsSettings.isImmutable("desktopChangeOsdEnabled")

                    checked: kcm.virtualDesktopsSettings.desktopChangeOsdEnabled

                    onToggled: kcm.virtualDesktopsSettings.desktopChangeOsdEnabled = checked
                }

                QQC2.SpinBox {
                    id: osdDuration

                    enabled: osdEnabled.checked && !kcm.virtualDesktopsSettings.isImmutable("popupHideDelay")

                    from: 0
                    to: 10000
                    stepSize: 100

                    textFromValue: function(value, locale) { return i18n("%1 ms", value)}

                    value: kcm.virtualDesktopsSettings.popupHideDelay

                    onValueModified: kcm.virtualDesktopsSettings.popupHideDelay = value
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    width: units.largeSpacing
                }

                QQC2.CheckBox {
                    id: osdTextOnly
                    enabled: osdEnabled.checked && !kcm.virtualDesktopsSettings.isImmutable("textOnly")
                    text: i18n("Show desktop layout indicators")
                    checked: !kcm.virtualDesktopsSettings.textOnly
                    onToggled: kcm.virtualDesktopsSettings.textOnly = !checked
                }
            }
        }
    }
}

