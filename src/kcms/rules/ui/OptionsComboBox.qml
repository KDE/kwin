/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami 2.10 as Kirigami
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

    // For multiple choice options we need to manually handle the Key events,
    // so forward them to the current delegate
    // HACK to reach it: ComboBox -> PopUp -> ScrollView -> ListView -> Delegate
    Keys.forwardTo: (down && multipleChoice) ? [popup.contentItem.contentItem.currentItem] : []

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
            onClicked: delegateItem.toggle()
        }

        // Space toggles an item and keeps the popup open
        Keys.onSpacePressed: event => {
            delegateItem.toggle();
        }

        function toggle() {
            if (model.optionType === OptionsModel.ExclusiveOption) {
                // Radio Button. Activate only the exclusive option
                selectionMask = model.bitMask;
            } else {
                // CheckBox. Toggle the option and add it to the mask
                checkBox.toggle();
                selectionMask = (checkBox.checked) ? selectionMask | model.bitMask : selectionMask & ~model.bitMask;
                selectionMask &= allOptionsMask;
            }
            activated(index);
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
