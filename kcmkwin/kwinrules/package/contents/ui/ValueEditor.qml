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
import org.kde.kquickcontrols 2.0 as KQC
import org.kde.kcms.kwinrules 1.0


Loader {
    id: valueEditor
    focus: true

    property var ruleValue
    property var ruleOptions
    property int controlType

    signal valueEdited(var value)

    sourceComponent: {
        switch (controlType) {
            case RuleItem.Boolean: return booleanEditor
            case RuleItem.String: return stringEditor
            case RuleItem.Integer: return integerEditor
            case RuleItem.Option: return optionEditor
            case RuleItem.FlagsOption: return flagsEditor
            case RuleItem.Percentage: return percentageEditor
            case RuleItem.Point: return coordinateEditor
            case RuleItem.Size: return coordinateEditor
            case RuleItem.Shortcut: return shortcutEditor
            default: return emptyEditor
        }
    }

    Component {
        id: emptyEditor
        Item {}
    }

    Component {
        id: booleanEditor
        RowLayout {
            Item {
                Layout.fillWidth: true
            }
            QQC2.RadioButton {
                text: i18n("Yes")
                checked: ruleValue
                Layout.margins: Kirigami.Units.smallSpacing
                onToggled: valueEditor.valueEdited(checked)
            }
            QQC2.RadioButton {
                text: i18n("No")
                checked: !ruleValue
                Layout.margins: Kirigami.Units.smallSpacing
                onToggled: valueEditor.valueEdited(!checked)
            }
        }
    }

    Component {
        id: stringEditor
        QQC2.TextField {
            property bool isTextEdited: false
            text: ruleValue
            horizontalAlignment: Text.AlignLeft
            onTextEdited: { isTextEdited = true; }
            onEditingFinished: {
                if (isTextEdited) { valueEditor.valueEdited(text); }
                isTextEdited = false;
            }
        }
    }

    Component {
        id: integerEditor
        QQC2.SpinBox {
            editable: true
            value: ruleValue
            onValueModified: valueEditor.valueEdited(value)
        }
    }

    Component {
        id: optionEditor
        OptionsComboBox {
            flat: true
            model: ruleOptions
            onActivated: (index) => {
                valueEditor.valueEdited(currentValue);
            }
        }
    }

    Component {
        id: flagsEditor
        OptionsComboBox {
            flat: true
            model: ruleOptions
            multipleChoice: true
            selectionMask: ruleValue
            onActivated: {
                valueEditor.valueEdited(selectionMask);
            }
        }
    }

    Component {
        id: percentageEditor
        RowLayout {
            QQC2.Slider {
                id: slider
                Layout.fillWidth: true
                from: 0
                to: 100
                value: ruleValue
                onMoved: valueEditor.valueEdited(Math.round(slider.value))
            }
            QQC2.Label {
                text: i18n("%1 %", Math.round(slider.value))
                horizontalAlignment: Qt.AlignRight
                Layout.minimumWidth: maxPercentage.width + Kirigami.Units.smallSpacing
                Layout.margins: Kirigami.Units.smallSpacing
            }
            TextMetrics {
                id: maxPercentage
                text: i18n("%1 %", 100)
            }
        }
    }

    Component {
        id: coordinateEditor
        RowLayout {
            id: coordItem
            spacing: Kirigami.Units.smallSpacing

            readonly property var coord: (controlType == RuleItem.Size) ? Qt.size(coordX.value, coordY.value)
                                                                        : Qt.point(coordX.value, coordY.value)
            onCoordChanged: valueEditor.valueEdited(coord)

            QQC2.SpinBox {
                id: coordX
                editable: true
                Layout.preferredWidth: 50   // 50%
                Layout.fillWidth: true
                from: 0
                to: 32767
                value: (controlType == RuleItem.Size) ? ruleValue.width : ruleValue.x
            }
            QQC2.Label {
                id: coordSeparator
                Layout.preferredWidth: implicitWidth
                text: i18nc("(x, y) coordinates separator in size/position","x")
                horizontalAlignment: Text.AlignHCenter
            }
            QQC2.SpinBox {
                id: coordY
                editable: true
                from: 0
                to: 32767
                Layout.preferredWidth: 50   // 50%
                Layout.fillWidth: true
                value: (controlType == RuleItem.Size) ? ruleValue.height : ruleValue.y
            }
        }
    }

    Component {
        id: shortcutEditor
        RowLayout {
            Item {
                Layout.fillWidth: true
            }
            KQC.KeySequenceItem {
                keySequence: ruleValue
                onCaptureFinished: valueEditor.valueEdited(keySequence)
            }
        }
    }
}
