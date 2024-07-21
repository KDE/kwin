/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kitemmodels
import org.kde.kcms.kwinrules


KCM.ScrollViewKCM {
    id: rulesEditor

    enum FilterMode {
        Properties = 0, // Only window properties that can get modified by the rule
        Conditions, // Only conditions to match a window
        Suggestions // All properties that have a suggested value
    }

    title: kcm.rulesModel.description

    view: ListView {
        id: rulesView
        clip: true

        model: enabledRulesModel

        delegate: RuleItemDelegate {
            Kirigami.Theme.inherit: model.section !== kcm.rulesModel.conditionsSection
            Kirigami.Theme.colorSet: Kirigami.Theme.Window

            ListView.onAdd: {
                // Try to position the new added item into the visible view
                // FIXME: It only works when moving towards the end of the list
                ListView.view.currentIndex = index
            }
        }
        section {
            property: "section"
            delegate: Kirigami.ListSectionHeader {
                width: ListView.view.width
                label: section

                Kirigami.Theme.inherit: section !== kcm.rulesModel.conditionsSection
                Kirigami.Theme.colorSet: Kirigami.Theme.Window

                QQC2.ToolButton {
                    visible: section === kcm.rulesModel.conditionsSection
                    text : i18n("Add Condition...")
                    icon.name: "list-add"

                    onClicked: {
                        overlayModel.mode = RulesEditor.FilterMode.Conditions;
                        propertySheet.visible = true;
                    }
                }
            }
        }

        highlightRangeMode: ListView.ApplyRange

        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: Kirigami.Units.longDuration * 3 }
        }
        removeDisplaced: Transition {
            NumberAnimation { property: "y"; duration: Kirigami.Units.longDuration }
        }

        // Necessary to get correct updates after the items are created
        onCountChanged: enabledRulesModel.conditionsCountChanged()

        // Background for the Window Matching Conditions section
        Rectangle {
            id: conditionsBackground

            parent: rulesView.contentItem
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }

            Kirigami.Theme.inherit: false
            Kirigami.Theme.colorSet: Kirigami.Theme.Window
            color: Kirigami.Theme.backgroundColor
            z: -1

            height: {
                const lastConditionItem =  rulesView.itemAtIndex(enabledRulesModel.conditionsCount - 1);
                if (!lastConditionItem) {
                    return 0;
                }
                return lastConditionItem.y + lastConditionItem.height;
            }

            Kirigami.Separator {
                anchors {
                    top: parent.bottom
                    left: parent.left
                    right: parent.right
                }
            }
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
            visible: rulesView.count <= enabledRulesModel.conditionsCount

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
            text: i18n("Add Property...")
            icon.name: "list-add"
            onClicked: {
                overlayModel.mode = RulesEditor.FilterMode.Properties;
                propertySheet.visible = true;
            }
        }
        Item {
            Layout.fillWidth: true
        }
        QQC2.Button {
            id: detectButton
            text: i18n("Detect Window Properties")
            icon.name: "edit-find"
            enabled: !propertySheet.visible && !errorDialog.visible
            onClicked: {
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
            overlayModel.mode = RulesEditor.FilterMode.Suggestions;
            propertySheet.visible = true;
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

        title: {
            switch (overlayModel.mode) {
            case RulesEditor.FilterMode.Suggestions:
                return i18n("Add detected properties or conditions");
            case RulesEditor.FilterMode.Conditions:
                return i18n("Add condition to the rule");
            case RulesEditor.FilterMode.Properties:
            default:
                return i18n("Add property to the rule");
            }
        }

        footer: Kirigami.SearchField {
            id: searchField
            horizontalAlignment: Text.AlignLeft
        }

        ListView {
            id: overlayView
            model: overlayModel
            Layout.preferredWidth: Kirigami.Units.gridUnit * 28
            clip: true
            reuseItems: true

            section {
                property: "section"
                delegate: Kirigami.ListSectionHeader {
                    label: section
                    width: ListView.view.width
                    height: implicitHeight
                }
            }

            delegate: QQC2.ItemDelegate {
                id: propertyDelegate
                highlighted: false
                width: ListView.view.width

                contentItem: RowLayout {
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

                    Kirigami.ContextualHelpButton {
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        visible: model.description.length > 0
                        toolTipText: model.description
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
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                visible: overlayModel.count === 0
                text: {
                    const isConditionMode = (overlayModel.mode === RulesEditor.FilterMode.Conditions);

                    if (searchField.text.length === 0) {
                        return (isConditionMode) ? i18nc("@info:placeholder", "No conditions left to add")
                                                 : i18nc("@info:placeholder", "No properties left to add");
                    }

                    return (isConditionMode)
                        ? i18nc("@info:placeholder %1 is a filter text introduced by the user", "No conditions match \"%1\"", searchField.text)
                        : i18nc("@info:placeholder %1 is a filter text introduced by the user", "No properties match \"%1\"", searchField.text);
                }
            }
        }

        onVisibleChanged: {
            searchField.text = "";
            if (visible) {
                searchField.forceActiveFocus();
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
                const selectedValue = value.toString(2).length - 1;
                return options.textOfValue(selectedValue);
            case RuleItem.OptionList:
                return Array.from(value, item => options.textOfValue(item) ).join(", ");
        }
        return value;
    }

    KSortFilterProxyModel {
        id: enabledRulesModel

        readonly property int conditionsCount: {
            for (let row = 0; row < count; row++) {
                if (data(index(row, 0), RulesModel.SectionRole) !== kcm.rulesModel.conditionsSection) {
                    return row;
                }
            }
            return count;
        }
        onModelReset: conditionsCountChanged()

        sourceModel: kcm.rulesModel
        filterRowCallback: (source_row, source_parent) => {
            const index = sourceModel.index(source_row, 0, source_parent);
            return sourceModel.data(index, RulesModel.EnabledRole);
        }
    }

    KSortFilterProxyModel {
        id: overlayModel
        sourceModel: kcm.rulesModel

        property int mode: RulesEditor.FilterMode.Properties
        onModeChanged: {
            invalidateFilter();
        }

        filterString: searchField.text.trim().toLowerCase()
        filterRowCallback: (source_row, source_parent) => {
            const index = sourceModel.index(source_row, 0, source_parent);

            // Mode filter
            const suggestedValue = sourceModel.data(index, RulesModel.SuggestedValueRole);
            if (mode === RulesEditor.FilterMode.Suggestions) {
                if (suggestedValue == null) {
                    return false;
                }
            } else {
                // Reject properties already added unless they have a different suggested value
                const isOptional = sourceModel.data(index, RulesModel.SelectableRole);
                const isEnabled = sourceModel.data(index, RulesModel.EnabledRole);
                const value = sourceModel.data(index, RulesModel.ValueRole);
                if ((isEnabled || !isOptional) && (suggestedValue === value || suggestedValue == null)) {
                    return false;
                }

                // Reject properties that doesn't match the mode
                const isCondition = sourceModel.data(index, RulesModel.SectionRole) == kcm.rulesModel.conditionsSection;
                if (isCondition !== (mode === RulesEditor.FilterMode.Conditions)) {
                    return false;
                }
            }

            // Text filter
            if (filterString.length > 0) {
                return sourceModel.data(index, RulesModel.NameRole).toLowerCase().includes(filterString)
            }
            return true;
        }
    }

}
