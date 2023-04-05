/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14 as QQC2
import org.kde.kirigami 2.10 as Kirigami

Kirigami.AbstractListItem {
    id: ruleDelegate

    property bool ruleEnabled: model.enabled

    Kirigami.Theme.colorSet: Kirigami.Theme.View

    width: ListView.view.width
    background: null
    highlighted: false
    hoverEnabled: true

    RowLayout {

        Kirigami.Icon {
            id: itemIcon
            source: model.icon
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.rightMargin: Kirigami.Units.smallSpacing
            Layout.alignment: Qt.AlignVCenter
        }

        QQC2.Label {
            text: model.name
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight
            Layout.preferredWidth: 10 * Kirigami.Units.gridUnit
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            QQC2.ToolTip.visible: ruleDelegate.hovered && (model.description.length > 0)
            QQC2.ToolTip.text: model.description
        }

        RowLayout {
            // This layout keeps the width constant between delegates, independent of items visibility
            Layout.fillWidth: true
            Layout.preferredWidth: 20 * Kirigami.Units.gridUnit
            Layout.minimumWidth: 13 * Kirigami.Units.gridUnit

            OptionsComboBox {
                id: policyCombo
                Layout.preferredWidth: 50  // 50%
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter

                visible: count > 0
                enabled: ruleEnabled

                model: policyModel
                onActivated: {
                    policy = currentValue;
                }
            }

            ValueEditor {
                id: valueEditor
                Layout.preferredWidth: 50  // 50%
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                enabled: model.enabled

                ruleValue: model.value
                ruleOptions: model.options
                controlType: model.type

                onValueEdited: (value) => {
                    model.value = value;
                }
            }

            QQC2.ToolButton {
                id: itemEnabled
                icon.name: "edit-delete"
                visible: model.selectable
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    model.enabled = false;
                }
            }
        }
    }
}
