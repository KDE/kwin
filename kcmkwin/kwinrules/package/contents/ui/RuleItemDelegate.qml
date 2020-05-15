/*
 * Copyright (c) 2020 Ismael Asensio <isma.af@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14 as QQC2
import org.kde.kirigami 2.10 as Kirigami

Kirigami.AbstractListItem {
    id: ruleDelegate

    property bool ruleEnabled: model.enabled

    Kirigami.Theme.colorSet: Kirigami.Theme.View

    width: ListView.view.width
    highlighted: false
    hoverEnabled: false

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
                flat: true

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

        QQC2.ToolTip {
            text: model.description
            visible: hovered && (text.length > 0)
        }
    }
}
