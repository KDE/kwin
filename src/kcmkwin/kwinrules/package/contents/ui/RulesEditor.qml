/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.19 as Kirigami
import org.kde.kcm 1.2
import org.kde.kitemmodels 1.0
import org.kde.kcms.kwinrules 1.0


ScrollViewKCM {
    id: rulesEditor

    title: kcm.rulesModel.description

    view: ListView {
        id: rulesView
        clip: true

        model: enabledRulesModel
        delegate: RuleItemDelegate {
            ListView.onAdd: {
                // Try to position the new added item into the visible view
                // FIXME: It only works when moving towards the end of the list
                ListView.view.currentIndex = index
            }
        }
        section {
            property: "section"
            delegate: Kirigami.ListSectionHeader { label: section }
        }

        highlightRangeMode: ListView.ApplyRange

        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: Kirigami.Units.longDuration * 3 }
        }
        removeDisplaced: Transition {
            NumberAnimation { property: "y"; duration: Kirigami.Units.longDuration }
        }

        // We need to center on the free space below contentItem, not the full
        // ListView. This invisible item helps make that positioning work no
        // matter the window height
        Item {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.contentItem.bottom
                bottom: parent.bottom
            }
            visible: rulesView.count <= 4

            Kirigami.PlaceholderMessage {
                id: hintArea
                anchors.centerIn: parent
                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                text: i18n("No window properties changed")
                explanation: xi18nc("@info", "Click the <interface>Add Property...</interface> button below to add some window properties that will be affected by the rule")
            }
        }
    }

    header: ColumnLayout {
        visible: warningList.count > 0
        Repeater {
            id: warningList
            model: kcm.rulesModel.warningMessages

            delegate: Kirigami.InlineMessage {
                text: modelData
                visible: true
                Layout.fillWidth: true
            }
        }
    }

    footer:  RowLayout {
        QQC2.Button {
            text: checked ? i18n("Close") : i18n("Add Property...")
            icon.name: checked ? "dialog-close" : "list-add"
            checkable: true
            checked: propertySheet.sheetOpen
            onToggled: {
                propertySheet.sheetOpen = checked;
            }
        }
        Item {
            Layout.fillWidth: true
        }
        QQC2.Button {
            id: detectButton
            text: i18n("Detect Window Properties")
            icon.name: "edit-find"
            enabled: !propertySheet.sheetOpen && !errorDialog.visible
            onClicked: {
                overlayModel.onlySuggestions = true;
                kcm.rulesModel.detectWindowProperties(Math.max(delaySpin.value * 1000,
                                                               Kirigami.Units.shortDuration));
            }
        }
        QQC2.SpinBox {
            id: delaySpin
            enabled: detectButton.enabled
            Layout.preferredWidth: Math.max(metricsInstant.advanceWidth, metricsAfter.advanceWidth) + Kirigami.Units.gridUnit * 4
            from: 0
            to: 30
            textFromValue: (value, locale) => {
                return (value == 0) ? i18n("Instantly")
                                    : i18np("After %1 second", "After %1 seconds", value)
            }

            TextMetrics {
                id: metricsInstant
                font: delaySpin.font
                text: i18n("Instantly")
            }
            TextMetrics {
                id: metricsAfter
                font: delaySpin.font
                text: i18np("After %1 second", "After %1 seconds", 99)
            }
        }

    }

    Connections {
        target: kcm.rulesModel
        function onShowSuggestions() {
            if (errorDialog.visible) {
                return;
            }
            overlayModel.onlySuggestions = true;
            propertySheet.sheetOpen = true;
        }
        function onShowErrorMessage(title, message) {
            errorDialog.title = title
            errorDialog.message = message
            errorDialog.open()
        }
    }

    Kirigami.Dialog {
        id: errorDialog

        property alias message: errorLabel.text

        preferredWidth: rulesEditor.width - Kirigami.Units.gridUnit * 6
        maximumWidth: Kirigami.Units.gridUnit * 35
        footer: null  // Just use the close button on top

        ColumnLayout {
            // Wrap it in a Layout so we can apply margins to the text while keeping implicit sizes
            Kirigami.Heading {
                id: errorLabel
                level: 4
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
            }
        }
    }

    Kirigami.OverlaySheet {
        id: propertySheet

        parent: view

        title: i18n("Add property to the rule")

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
                delegate: Kirigami.ListSectionHeader {
                    label: section
                    height: implicitHeight
                }
            }

            delegate: Kirigami.AbstractListItem {
                id: propertyDelegate
                highlighted: false
                width: ListView.view.width

                RowLayout {
                    Kirigami.Icon {
                        source: model.icon
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
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
                            visible: hovered && (model.description.length > 0)
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
                        icon.name: (model.enabled) ? "dialog-ok-apply" : "list-add"
                        onClicked: addProperty();
                        Layout.preferredWidth: implicitWidth
                        Layout.leftMargin: -Kirigami.Units.smallSpacing
                        Layout.rightMargin: -Kirigami.Units.smallSpacing
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                onClicked: {
                    addProperty();
                    propertySheet.close();
                }

                function addProperty() {
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
            case RuleItem.Point:
                return i18nc("Coordinates (x, y)", "(%1, %2)", value.x, value.y);
            case RuleItem.Size:
                return i18nc("Size (width, height)", "(%1, %2)", value.width, value.height);
            case RuleItem.Option:
                return options.textOfValue(value);
            case RuleItem.NetTypes:
                var selectedValue = value.toString(2).length - 1;
                return options.textOfValue(selectedValue);
            case RuleItem.OptionList:
                return Array.from(value, item => options.textOfValue(item) ).join(", ");
        }
        return value;
    }

    KSortFilterProxyModel {
        id: enabledRulesModel
        sourceModel: kcm.rulesModel
        filterRowCallback: (source_row, source_parent) => {
            var index = sourceModel.index(source_row, 0, source_parent);
            return sourceModel.data(index, RulesModel.EnabledRole);
        }
    }

    KSortFilterProxyModel {
        id: overlayModel
        sourceModel: kcm.rulesModel

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
            if (filterString.length > 0) {
                return sourceModel.data(index, RulesModel.NameRole).toLowerCase().includes(filterString)
            }
            return true;
        }
    }

}
