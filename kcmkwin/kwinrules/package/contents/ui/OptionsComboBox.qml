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


QQC2.ComboBox {
    id: optionsCombo

    textRole: "display"
    //TODO: After KF5 (qqc2-desktop-style) depends on Qt 5.15 this can be simplified using newer API
    //      (https://bugs.kde.org/show_bug.cgi?id=419521)
    //      valueRole: "value"
    property var currentValue

    onActivated: (index) => {
        var modelIndex = model.index(index, 0);
        currentValue = model.data(modelIndex, Qt.UserRole);
    }

    property bool multipleChoice: false
    property int selectionMask: 0

    currentIndex: multipleChoice ? -1 : model.selectedIndex

    displayText: {
        if (!multipleChoice) {
            return currentText;
        }
        var selectionCount = selectionMask.toString(2).replace(/0/g, '').length;
        switch (selectionCount) {
            case 0:
                return i18n("None selected");
            case 1:
                var selectedValue = selectionMask.toString(2).length - 1;
                return model.textOfValue(selectedValue);
            case count:
                return i18n("All selected");
        }
        return i18np("%1 selected", "%1 selected", selectionCount);
    }

    delegate: QQC2.ItemDelegate {
        highlighted: optionsCombo.highlightedIndex == index
        width: parent.width

        contentItem: RowLayout {
            QQC2.CheckBox {
                id: itemSelection
                visible: multipleChoice
                checked: (selectionMask & (1 << value))
                onToggled: {
                    selectionMask = (selectionMask & ~(1 << value)) | (checked << value);
                    activated(index);
                }
            }
            Kirigami.Icon {
                source: model.decoration
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
            }
            QQC2.Label {
                text: model.display
                color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft
            }
        }

        MouseArea {
            anchors.fill: contentItem
            enabled: multipleChoice
            onClicked: {
                itemSelection.toggle();
                itemSelection.toggled();
            }
        }

        QQC2.ToolTip {
            text: model.tooltip
            visible: hovered && (model.tooltip != "")
        }

        Component.onCompleted: {
            //FIXME: work around bug https://bugs.kde.org/show_bug.cgi?id=403153
            optionsCombo.popup.width = Math.max(implicitWidth, optionsCombo.width, optionsCombo.popup.width);
        }

        onActiveFocusChanged: {
            if (!activeFocus) {
                popup.close();
            }
        }
    }
}
