/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQml.Models
import org.kde.kcmutils as KCM
import org.kde.kirigami 2.12 as Kirigami

KCM.ScrollViewKCM {
    id: rulesListKCM

    // FIXME: ScrollViewKCM.qml:73:13: QML Control: Binding loop detected for property "implicitHeight"
    implicitWidth: Kirigami.Units.gridUnit * 35
    implicitHeight: Kirigami.Units.gridUnit * 25

    KCM.ConfigModule.columnWidth: Kirigami.Units.gridUnit * 23
    KCM.ConfigModule.buttons: KCM.ConfigModule.Help | KCM.ConfigModule.Apply

    property var selectedIndexes: []

    actions: [
        Kirigami.Action {
            enabled: !exportInfo.visible
            text: i18n("Add New…")
            icon.name: "list-add-symbolic"
            onTriggered: kcm.createRule();
        },
        Kirigami.Action {
            enabled: !exportInfo.visible
            text: i18n("Import…")
            icon.name: "document-import-symbolic"
            onTriggered: importDialog.active = true;
        },
        Kirigami.Action {
            text: checked ? i18n("Cancel Export") : i18n("Export…")
            icon.name: exportInfo.visible ? "dialog-cancel-symbolic" : "document-export-symbolic"
            checkable: true
            checked: exportInfo.visible
            onToggled: {
                selectedIndexes = [];
                exportInfo.visible = checked;
            }
        }
    ]

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
            NumberAnimation {
                properties: "y"
                duration: Kirigami.Units.longDuration
            }
        }

        Kirigami.PlaceholderMessage {
            visible: ruleBookView.count === 0
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            icon.name: "preferences-system-windows-actions"
            text: i18n("No rules for specific windows are currently set");
            explanation: xi18nc("@info", "Click <interface>Add New…</interface> to add some")
        }
    }

    header: Kirigami.InlineMessage {
        id: exportInfo
        icon.source: "document-export"
        showCloseButton: true
        text: i18n("Select the rules to export")
        actions: [
            Kirigami.Action {
                icon.name: "dialog-ok-apply"
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
                icon.name: "document-save"
                text: i18n("Save Rules")
                enabled: selectedIndexes.length > 0
                onTriggered: {
                    exportDialog.active = true;
                }
            }
        ]
    }

    component RuleBookDelegate : Item {
        // External item required to make Kirigami.ListItemDragHandle work
        width: ruleBookView.width
        implicitHeight: ruleBookItem.implicitHeight

        ListView.onPooled: {
            if (descriptionField.activeFocus) {
                // If the description was being edited when the item is pooled, finish the edition
                ruleBookItem.forceActiveFocus();
            }
        }

        QQC2.ItemDelegate {
            id: ruleBookItem

            width: ruleBookView.width
            down: false  // Disable press effect

            contentItem: RowLayout {
                Kirigami.ListItemDragHandle {
                    visible: !exportInfo.visible
                    listItem: ruleBookItem
                    listView: ruleBookView
                    onMoveRequested: (oldIndex, newIndex) => {
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
                    Keys.onPressed: event => {
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

                DelegateButton {
                    text: i18n("Edit")
                    icon.name: "edit-entry"
                    onClicked: kcm.editRule(index);
                }

                DelegateButton {
                    text: i18n("Duplicate")
                    icon.name: "edit-duplicate"
                    onClicked: kcm.duplicateRule(index);
                }

                DelegateButton {
                    text: i18n("Delete")
                    icon.name: "entry-delete"
                    onClicked: kcm.removeRule(index);
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
        }
    }

    component DelegateButton: QQC2.ToolButton {
        visible: !exportInfo.visible
        display: QQC2.AbstractButton.IconOnly
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
    }

    FileDialogLoader {
        id: importDialog
        title: i18n("Import Rules")
        isSaveDialog: false
        onLastFolderChanged: {
            exportDialog.lastFolder = lastFolder;
        }
        onFileSelected: path => {
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
        onFileSelected: path => {
            selectedIndexes.sort();
            kcm.exportToFile(path, selectedIndexes);
            exportInfo.visible = false;
        }
    }
}
