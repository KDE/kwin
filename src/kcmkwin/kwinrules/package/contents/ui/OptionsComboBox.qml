/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14 as QQC2

import org.kde.kirigami 2.10 as Kirigami
import org.kde.kcms.kwinrules 1.0


QQC2.ComboBox {
    id: optionsCombo

    textRole: "display"
    valueRole: "value"

    property bool multipleChoice: false
    // If `useFlagsValue` is true, `selectionMask` will be composed using the item values.
    // Otherwise, it will use the item indexes.
    property bool useFlagsValue: false
    property int selectionMask: 0
    readonly property int selectionCount: selectionMask.toString(2).replace(/0/g, '').length

    property int globalBit: -1
    property int selectAllBit: -1

    currentIndex: multipleChoice ? -1 : model.selectedIndex

    displayText: {
        if (!multipleChoice) {
            return currentText;
        }
        switch (selectionCount) {
            case 0:
                return i18n("None selected");
            case 1:
                let selectedBit = selectionMask.toString(2).length - 1;
                let selectedIndex = (useFlagsValue) ? model.indexOf(selectedBit) : selectedBit
                return model.data(model.index(selectedIndex, 0), Qt.DisplayRole);
            case count:
                return i18n("All selected");
        }
        return i18np("%1 selected", "%1 selected", selectionCount);
    }

    delegate: QQC2.ItemDelegate {
        highlighted: optionsCombo.highlightedIndex == index
        width: parent.width

        contentItem: RowLayout {
            Loader {
                id: itemSelection
                active: multipleChoice
                visible: active
                readonly property int bit: (useFlagsValue) ? value : index
                readonly property bool checked: (selectionMask & (1 << bit))
                sourceComponent: model.optionType === OptionsModel.GlobalType ? radioButton : checkBox

                Component {
                    id: radioButton

                    QQC2.RadioButton {
                        checked: itemSelection.checked
                        onToggled: {
                            selectionMask = (checked << bit); // Only check the global option item
                            activated(index);
                        }

                        Component.onCompleted: optionsCombo.globalBit = Qt.binding(() => itemSelection.bit);
                    }
                }

                Component {
                    id: checkBox

                    QQC2.CheckBox {
                        checked: itemSelection.checked
                        onToggled: {
                            if (model.optionType === OptionsModel.SelectAllType) {
                                selectionMask = 0; // Reset the mask
                                if (checked) {
                                    for (let i = 0; i < optionsCombo.count; i++) {
                                        selectionMask = selectionMask | (1 << i); // Check all items
                                    }
                                }
                            } else {
                                if (optionsCombo.globalBit >= 0) {
                                    selectionMask = (selectionMask & ~(1 << optionsCombo.globalBit)); // Uncheck "Global"
                                } else if (optionsCombo.selectAllBit >= 0) {
                                    selectionMask = (selectionMask & ~(1 << optionsCombo.selectAllBit));
                                }
                                selectionMask = (selectionMask & ~(1 << bit)) | (checked << bit);
                            }

                            if (optionsCombo.selectionCount === count - 1) {
                                if (optionsCombo.globalBit >= 0) {
                                    selectionMask = (1 << optionsCombo.globalBit); // Only check the global option item
                                    activated(optionsCombo.globalBit);
                                    return;
                                } else if (optionsCombo.selectAllBit >= 0) {
                                    selectionMask = selectionMask | (1 << optionsCombo.selectAllBit); // Check "Select all"
                                    activated(optionsCombo.selectAllBit);
                                    return;
                                }
                            }

                            activated(index);
                        }

                        Component.onCompleted: if (model.optionType === OptionsModel.SelectAllType) {
                            optionsCombo.selectAllBit = Qt.binding(() => itemSelection.bit);
                        }
                    }
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
                itemSelection.item.toggle();
                itemSelection.item.toggled();
            }
        }

        QQC2.ToolTip {
            text: model.tooltip
            visible: hovered && (model.tooltip.length > 0)
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
