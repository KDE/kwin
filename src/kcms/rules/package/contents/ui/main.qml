/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14 as QQC2
import QtQml.Models 2.14
import org.kde.kcm 1.2
import org.kde.kirigami 2.12 as Kirigami

ScrollViewKCM {
    id: rulesListKCM

    // FIXME: ScrollViewKCM.qml:73:13: QML Control: Binding loop detected for property "implicitHeight"
    implicitWidth: Kirigami.Units.gridUnit * 35
    implicitHeight: Kirigami.Units.gridUnit * 25

    ConfigModule.columnWidth: Kirigami.Units.gridUnit * 23
    ConfigModule.buttons: ConfigModule.Help | ConfigModule.Apply

    property var selectedIndexes: []

    // Manage KCM pages
    Connections {
        target: kcm
        function onEditIndexChanged() {
            if (kcm.editIndex < 0) {
                // If no rule is being edited, hide RulesEdidor page
                kcm.pop();
            } else if (kcm.depth < 2) {
                // Add the RulesEditor page if it wasn't already
                kcm.push("RulesEditor.qml");
            }
        }
    }

    view: ListView {
        id: ruleBookView
        clip: true

        model: kcm.ruleBookModel
        currentIndex: kcm.editIndex
        delegate: RuleBookDelegate {}
        reuseItems: true

        highlightMoveDuration: Kirigami.Units.longDuration

        displaced: Transition {
            NumberAnimation { properties: "y"; duration: Kirigami.Units.longDuration }
        }

        Kirigami.PlaceholderMessage {
            visible: ruleBookView.count === 0
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            text: i18n("No rules for specific windows are currently set");
            explanation: xi18nc("@info", "Click the <interface>Add New...</interface> button below to add some")
        }
    }

    header: Kirigami.InlineMessage {
        id: exportInfo
        icon.source: "document-export"
        showCloseButton: true
        text: i18n("Select the rules to export")
        actions: [
            Kirigami.Action {
                iconName: "dialog-ok-apply"
                text: checked ? i18n("Unselect All") : i18n("Select All")
                checkable: true
                checked: selectedIndexes.length === ruleBookView.count
                onToggled: {
                    if (checked) {
                        selectedIndexes = [...Array(ruleBookView.count).keys()]
                    } else {
                        selectedIndexes = [];
                    }
                }
            }
            ,
            Kirigami.Action {
                iconName: "document-save"
                text: i18n("Save Rules")
                enabled: selectedIndexes.length > 0
                onTriggered: {
                    exportDialog.active = true;
                }
            }
        ]
    }

    footer: RowLayout {
        QQC2.Button {
            text: i18n("Add New...")
            icon.name: "list-add"
            enabled: !exportInfo.visible
            onClicked: {
                kcm.createRule();
            }
        }
        Item {
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: i18n("Import...")
            icon.name: "document-import"
            enabled: !exportInfo.visible
            onClicked: {
                importDialog.active = true;
            }
        }
        QQC2.Button {
            text: checked ? i18n("Cancel Export") : i18n("Export...")
            icon.name: exportInfo.visible ? "dialog-cancel" : "document-export"
            enabled: ruleBookView.count > 0
            checkable: true
            checked: exportInfo.visible
            onToggled: {
                selectedIndexes = []
                exportInfo.visible = checked;
            }
        }
    }

    component RuleBookDelegate : Item {
        // External item required to make Kirigami.ListItemDragHandle work
        width : ruleBookView.width
        implicitHeight : ruleBookItem.implicitHeight

        ListView.onPooled: {
            if (descriptionField.activeFocus) {
                // If the description was being edited when the item is pooled, finish the edition
                ruleBookItem.forceActiveFocus();
            }
        }

        Kirigami.SwipeListItem {
            id: ruleBookItem

            RowLayout {
                Kirigami.ListItemDragHandle {
                    visible: !exportInfo.visible
                    listItem: ruleBookItem
                    listView: ruleBookView
                    onMoveRequested: {
                        kcm.moveRule(oldIndex, newIndex);
                    }
                }

                QQC2.TextField {
                    id: descriptionField
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                    Layout.fillWidth: true
                    background: Item {}
                    horizontalAlignment: Text.AlignLeft
                    text: model && model.display
                    onEditingFinished: {
                        kcm.setRuleDescription(index, text);
                    }
                    Keys.onPressed: {
                        switch (event.key) {
                        case Qt.Key_Escape:
                            // On <Esc> key reset to model data before losing focus
                            text = model.display;
                        case Qt.Key_Enter:
                        case Qt.Key_Return:
                        case Qt.Key_Tab:
                            ruleBookItem.forceActiveFocus();
                            event.accepted = true;
                            break;
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: exportInfo.visible
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.IBeamCursor
                        onClicked: {
                            itemSelectionCheck.toggle();
                            itemSelectionCheck.toggled();
                        }
                    }
                }

                QQC2.CheckBox {
                    id: itemSelectionCheck
                    visible: exportInfo.visible
                    checked: selectedIndexes.includes(index)
                    onToggled: {
                        var position = selectedIndexes.indexOf(index);
                        if (checked) {
                            if (position < 0) { selectedIndexes.push(index); }
                        } else {
                            if (position >= 0) { selectedIndexes.splice(position, 1); }
                        }
                        selectedIndexesChanged();
                    }
                }
            }

            actions: [
                Kirigami.Action {
                    text: i18n("Edit")
                    iconName: "edit-entry"
                    visible: !exportInfo.visible
                    onTriggered: {
                        kcm.editRule(index);
                    }
                }
                ,
                Kirigami.Action {
                    text: i18n("Duplicate")
                    iconName: "edit-duplicate"
                    visible: !exportInfo.visible
                    onTriggered: {
                        kcm.duplicateRule(index);
                    }
                }
                ,
                Kirigami.Action {
                    text: i18n("Delete")
                    iconName: "entry-delete"
                    visible: !exportInfo.visible
                    onTriggered: {
                        kcm.removeRule(index);
                    }
                }
            ]
        }
    }

    FileDialogLoader {
        id: importDialog
        title: i18n("Import Rules")
        isSaveDialog: false
        onLastFolderChanged: {
            exportDialog.lastFolder = lastFolder;
        }
        onFileSelected: {
            kcm.importFromFile(path);
        }
    }

    FileDialogLoader {
        id: exportDialog
        title: i18n("Export Rules")
        isSaveDialog: true
        onLastFolderChanged: {
            importDialog.lastFolder = lastFolder;
        }
        onFileSelected: {
            selectedIndexes.sort();
            kcm.exportToFile(path, selectedIndexes);
            exportInfo.visible = false;
        }
    }
}
