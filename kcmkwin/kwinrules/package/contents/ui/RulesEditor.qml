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
import org.kde.kcm 1.2
import org.kde.kitemmodels 1.0
import org.kde.kcms.kwinrules 1.0


ScrollViewKCM {
    id: rulesEditor

    property var rulesModel: kcm.rulesModel

    title: rulesModel.description

    view: ListView {
        id: rulesView
        clip: true

        model: enabledRulesModel
        delegate: RuleItemDelegate {}
        section {
            property: "section"
            delegate: Kirigami.ListSectionHeader { label: section }
        }

        Item {
            id: hintArea
            visible: rulesView.count <= 4
            anchors {
                top: parent.contentItem.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            QQC2.Button {
                anchors.centerIn: parent
                text: i18n("Add Properties...")
                icon.name: "list-add-symbolic"
                onClicked: {
                    propertySheet.open();
                }
            }
        }
    }

    // FIXME: InlineMessage.qml:241:13: QML Label: Binding loop detected for property "verticalAlignment"
    header: Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.fillHeight: true
        text: rulesModel.warningMessage
        visible: text != ""
    }

    footer:  RowLayout {
        QQC2.Button {
            text: checked ? i18n("Close") : i18n("Add Properties...")
            icon.name: checked ? "dialog-close" : "list-add-symbolic"
            checkable: true
            checked: propertySheet.sheetOpen
            visible: !hintArea.visible || checked
            onToggled: {
                propertySheet.sheetOpen = checked;
            }
        }
        Item {
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: i18n("Detect Window Properties")
            icon.name: "edit-find"
            onClicked: {
                overlayModel.onlySuggestions = true;
                rulesModel.detectWindowProperties(delaySpin.value);
            }
        }
        QQC2.SpinBox {
            id: delaySpin
            Layout.preferredWidth: Kirigami.Units.gridUnit * 8
            from: 0
            to: 30
            textFromValue: (value, locale) => {
                return (value == 0) ? i18n("Instantly")
                                    : i18np("After %1 second", "After %1 seconds", value)
            }
        }

    }

    Connections {
        target: rulesModel
        onSuggestionsChanged: {
            propertySheet.sheetOpen = true;
        }
    }

    Kirigami.OverlaySheet {
        id: propertySheet

        parent: view

        header: Kirigami.Heading {
            text: i18n("Select properties")
        }
        footer: Kirigami.SearchField {
            id: searchField
            horizontalAlignment: Text.AlignLeft
        }

        ListView {
            id: overlayView
            model: overlayModel
            Layout.preferredWidth: Kirigami.Units.gridUnit * 28

            section {
                property: "section"
                delegate: Kirigami.ListSectionHeader { label: section }
            }

            delegate: Kirigami.AbstractListItem {
                id: propertyDelegate
                highlighted: false
                width: ListView.view.width

                RowLayout {
                    anchors.fill: parent

                    Kirigami.Icon {
                        source: model.icon
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC2.Label {
                        id: itemNameLabel
                        text: model.name
                        horizontalAlignment: Qt.AlignLeft
                        Layout.preferredWidth: implicitWidth
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        QQC2.ToolTip {
                            text: model.description
                            visible: hovered && (model.description != "")
                        }
                    }
                    QQC2.Label {
                        id: suggestedLabel
                        text: formatValue(model.suggested, model.type, model.options)
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                        opacity: 0.7
                        Layout.maximumWidth: propertyDelegate.width - itemNameLabel.implicitWidth - Kirigami.Units.gridUnit * 6
                        Layout.alignment: Qt.AlignVCenter
                        QQC2.ToolTip {
                            text: suggestedLabel.text
                            visible: hovered && suggestedLabel.truncated
                        }
                    }
                    QQC2.ToolButton {
                        icon.name: (model.enabled) ? "dialog-ok-apply" : "list-add-symbolic"
                        opacity: propertyDelegate.hovered ? 1 : 0
                        onClicked: propertyDelegate.clicked()
                        Layout.preferredWidth: implicitWidth
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                onClicked: {
                    model.enabled = true;
                    if (model.suggested != null) {
                        model.value = model.suggested;
                        model.suggested = null;
                    }
                }
            }
        }

        onSheetOpenChanged: {
            searchField.text = "";
            if (sheetOpen) {
                searchField.forceActiveFocus();
            } else {
                overlayModel.onlySuggestions = false;
            }
        }
    }

    function formatValue(value, type, options) {
        if (value == null) {
            return "";
        }
        switch (type) {
            case RuleItem.Boolean:
                return value ? i18n("Yes") : i18n("No");
            case RuleItem.Percentage:
                return i18n("%1 %", value);
            case RuleItem.Coordinate:
                var point = value.split(',');
                return i18nc("Coordinates (x, y)", "(%1, %2)", point[0], point[1]);
            case RuleItem.Option:
                return options.textOfValue(value);
            case RuleItem.FlagsOption:
                var selectedValue = value.toString(2).length - 1;
                return options.textOfValue(selectedValue);
        }
        return value;
    }

    KSortFilterProxyModel {
        id: enabledRulesModel
        sourceModel: rulesModel
        filterRowCallback: (source_row, source_parent) => {
            var index = sourceModel.index(source_row, 0, source_parent);
            return sourceModel.data(index, RulesModel.EnabledRole);
        }
    }

    KSortFilterProxyModel {
        id: overlayModel
        sourceModel: rulesModel

        property bool onlySuggestions: false
        onOnlySuggestionsChanged: {
            invalidateFilter();
        }

        filterString: searchField.text.trim().toLowerCase()
        filterRowCallback: (source_row, source_parent) => {
            var index = sourceModel.index(source_row, 0, source_parent);

            var hasSuggestion = sourceModel.data(index, RulesModel.SuggestedValueRole) != null;
            var isOptional = sourceModel.data(index, RulesModel.SelectableRole);
            var isEnabled = sourceModel.data(index, RulesModel.EnabledRole);

            var showItem = hasSuggestion || (!onlySuggestions && isOptional && !isEnabled);

            if (!showItem) {
                return false;
            }
            if (filterString != "") {
                return sourceModel.data(index, RulesModel.NameRole).toLowerCase().includes(filterString)
            }
            return true;
        }
    }

}
