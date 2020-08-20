/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14 as QQC2
import org.kde.kirigami 2.12 as Kirigami
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

        Kirigami.PlaceholderMessage {
            id: hintArea
            visible: rulesView.count <= 4
            anchors {
                // We need to center on the free space below contentItem, not the full ListView.
                // Setting both top and bottom anchors (or using anchors.fill) stretches the component
                // and distorts the spacing between its internal items.
                // This is fine as long as we have a single item here.
                horizontalCenter: parent.horizontalCenter
                top: parent.contentItem.bottom
                bottom: parent.bottom
            }
            width: parent.width - (units.largeSpacing * 4)
            helpfulAction: QQC2.Action {
                text: i18n("Add Property...")
                icon.name: "list-add-symbolic"
                onTriggered: {
                    propertySheet.open();
                }
            }
        }
    }

    header: Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.fillHeight: true
        text: rulesModel.warningMessage
        visible: text != ""
    }

    footer:  RowLayout {
        QQC2.Button {
            text: checked ? i18n("Close") : i18n("Add Property...")
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
            id: detectButton
            text: i18n("Detect Window Properties")
            icon.name: "edit-find"
            enabled: !propertySheet.sheetOpen && !errorSheet.sheetOpen
            onClicked: {
                overlayModel.onlySuggestions = true;
                rulesModel.detectWindowProperties(Math.max(delaySpin.value * 1000,
                                                           Kirigami.Units.shortDuration));
            }
        }
        QQC2.SpinBox {
            id: delaySpin
            enabled: detectButton.enabled
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
        function onShowSuggestions() {
            overlayModel.onlySuggestions = true;
            propertySheet.sheetOpen = true;
        }
        function onShowErrorMessage(message) {
            errorLabel.text = message
            errorSheet.open()
        }
    }

    Kirigami.OverlaySheet {
        id: errorSheet
        header: Kirigami.Heading {
            text: i18n("Error")
        }
        Kirigami.Heading {
            id: errorLabel
            level: 3
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    Kirigami.OverlaySheet {
        id: propertySheet

        parent: view

        header: Kirigami.Heading {
            text: i18n("Add property to the rule")
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
                        icon.name: (model.enabled) ? "dialog-ok-apply" : "list-add-symbolic"
                        opacity: propertyDelegate.hovered ? 1 : 0
                        onClicked: propertyDelegate.clicked()
                        Layout.preferredWidth: implicitWidth
                        Layout.leftMargin: -Kirigami.Units.smallSpacing
                        Layout.rightMargin: -Kirigami.Units.smallSpacing
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                onClicked: {
                    model.enabled = true;
                    if (model.suggested != null) {
                        model.value = model.suggested;
                        model.suggested = null;
                    }
                    if (!overlayModel.onlySuggestions) {
                        propertySheet.close();
                    }
                }
            }
        }

        onSheetOpenChanged: {
            searchField.text = "";
            if (sheetOpen) {
                overlayModel.ready = true;
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

        // Delay the model filtering until `ready` is set
        // FIXME: Workaround https://bugs.kde.org/show_bug.cgi?id=422289
        property bool ready: false
        onReadyChanged: {
            invalidateFilter();
        }

        filterString: searchField.text.trim().toLowerCase()
        filterRowCallback: (source_row, source_parent) => {
            if (!ready) {
                return false;
            }

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
