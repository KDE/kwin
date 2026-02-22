/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcms.kwinrules


QQC2.ComboBox {
    id: optionsCombo

    textRole: "display"
    valueRole: "value"

    property bool multipleChoice: false
    property int selectionMask: 0
    readonly property int allOptionsMask: model.allOptionsMask

    currentIndex: multipleChoice ? -1 : model.selectedIndex

    displayText: {
        if (!multipleChoice) {
            return currentText;
        }
        const selectionCount = selectionMask.toString(2).replace(/0/g, '').length;
        const optionsCount = allOptionsMask.toString(2).replace(/0/g, '').length;
        switch (selectionCount) {
            case 0:
                return i18n("None selected");
            case 1:
                const selectedBit = selectionMask.toString(2).length - 1;
                const selectedIndex = (model.useFlags) ? model.indexOf(selectionMask) : selectedBit
                return model.data(model.index(selectedIndex, 0), Qt.DisplayRole);
            case optionsCount:
                return i18n("All selected");
        }
        return i18np("%1 selected", "%1 selected", selectionCount);
    }

    function toggleOption(index: int) {
        const optionType = model.index(index, 0).data(OptionsModel.OptionTypeRole);
        const bitMask = model.index(index, 0).data(OptionsModel.BitMaskRole);

        if (optionType === OptionsModel.ExclusiveOption) {
            // Radio Button. Activate only the exclusive option
            selectionMask = bitMask;
        } else {
            // CheckBox. Toggle the option indicated by the mask
            const wasChecked = (selectionMask & bitMask) == bitMask
            if (wasChecked) {
                selectionMask &= ~bitMask;
            } else {
                selectionMask |= bitMask;
            }
            selectionMask &= allOptionsMask;
        }
        activated(index);
    }

    Keys.onSpacePressed: event => {
        if (down && multipleChoice) {
            toggleOption(highlightedIndex);
            event.accepted = true;
            return;
        }
        // Default to the regular event handling
        event.accepted = false;
    }

    delegate: QQC2.ItemDelegate {
        id: delegateItem

        highlighted: optionsCombo.highlightedIndex == index
        width: parent.width

        contentItem: RowLayout {
            QQC2.RadioButton {
                id: radioButton
                visible: multipleChoice && model.optionType === OptionsModel.ExclusiveOption
                checked: (selectionMask & bitMask) == bitMask
            }
            QQC2.CheckBox {
                id: checkBox
                visible: multipleChoice && model.optionType !== OptionsModel.ExclusiveOption
                checked: (selectionMask & model.bitMask) == model.bitMask
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
            onClicked: optionsCombo.toggleOption(index)
        }

        QQC2.ToolTip {
            text: model.tooltip
            visible: hovered && (model.tooltip.length > 0)
        }

        Component.onCompleted: {
            //FIXME: work around bug https://bugs.kde.org/show_bug.cgi?id=403153
            optionsCombo.popup.width = Math.max(implicitWidth, optionsCombo.width, optionsCombo.popup.width);
        }
    }
}
