/*
 * Copyright (C) 2018 Eike Hein <hein@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
*/

import QtQuick 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.4 as QtControls
import org.kde.kirigami 2.5 as Kirigami
import org.kde.plasma.core 2.1 as PlasmaCore
import org.kde.kcm 1.2

ScrollViewKCM {
    id: root

    ConfigModule.quickHelp: i18n("Virtual Desktops")

    Connections {
        target: kcm.desktopsModel

        onReadyChanged: {
            rowsSpinBox.value = kcm.desktopsModel.rows;
        }

        onRowsChanged: {
            rowsSpinBox.value = kcm.desktopsModel.rows;
        }
    }

    Component {
        id: desktopsListItemComponent

        Kirigami.SwipeListItem {
            id: listItem

            contentItem: RowLayout {
                QtControls.TextField {
                    id: nameField

                    background: null
                    leftPadding: Kirigami.Units.largeSpacing
                    topPadding: 0
                    bottomPadding: 0

                    Layout.fillWidth: true

                    Layout.alignment: Qt.AlignVCenter

                    text: model.display

                    readOnly: true

                    onEditingFinished: {
                        readOnly = true;
                        Qt.callLater(kcm.desktopsModel.setDesktopName, model.Id, text);
                    }
                }
            }

        actions: [
            Kirigami.Action {
                enabled: !model.IsMissing
                iconName: "edit-rename"
                tooltip: i18nc("@info:tooltip", "Rename")
                onTriggered: {
                    nameField.readOnly = false;
                    nameField.selectAll();
                    nameField.forceActiveFocus();
                }
            },
            Kirigami.Action {
                enabled: !model.IsMissing
                iconName: "list-remove"
                tooltip: i18nc("@info:tooltip", "Remove")
                onTriggered: kcm.desktopsModel.removeDesktop(model.Id)
            }]
        }
    }

    header: ColumnLayout {
        id: messagesLayout

        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true

            type: Kirigami.MessageType.Error

            text: kcm.desktopsModel.error

            visible: kcm.desktopsModel.error != ""
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true

            type: Kirigami.MessageType.Information

            text: i18n("Virtual desktops have been changed outside this settings application. Saving now will overwrite the changes.")

            visible: kcm.desktopsModel.serverModified
        }

        RowLayout {
            QtControls.Label {
                text: i18n("Rows:")
            }

            QtControls.SpinBox {
                id: rowsSpinBox

                from: 1
                to: 20

                onValueModified: kcm.desktopsModel.rows = value
            }

            Item { // Spacer
                Layout.fillWidth: true
            }

            QtControls.Button {
                Layout.alignment: Qt.AlignRight

                text: i18nc("@action:button", "Add")
                icon.name: "list-add"

                onClicked: kcm.desktopsModel.createDesktop(i18n("New Desktop"))
            }
        }
    }

    view: ListView {
        id: desktopsList

        model: kcm.desktopsModel.ready ? kcm.desktopsModel : null

        section.property: "DesktopRow"
        section.delegate: Kirigami.AbstractListItem {
            width: desktopsList.width

            backgroundColor: Kirigami.Theme.backgroundColor

            hoverEnabled: false
            supportsMouseEvents: false

            Kirigami.Theme.inherit: false
            Kirigami.Theme.colorSet: Kirigami.Theme.Window

            QtControls.Label {
                text: i18n("Row %1", section)
            }
        }

        delegate: Kirigami.DelegateRecycler {
            width: desktopsList.width

            sourceComponent: desktopsListItemComponent
        }
    }

    footer: Kirigami.FormLayout {
        Connections {
            target: kcm

            onNavWrapsChanged: navWraps.checked = kcm.navWraps

            onOsdEnabledChanged: osdEnabled.checked = kcm.osdEnabled
            onOsdDurationChanged: osdDuration.value = kcm.osdDuration
            onOsdTextOnlyChanged: osdTextOnly.checked = !kcm.osdTextOnly
        }

        QtControls.CheckBox {
            id: navWraps

            Kirigami.FormData.label: i18n("Options:")

            text: i18n("Navigation wraps around")

            checked: kcm.navWraps

            onCheckedChanged: kcm.navWraps = checked
        }

        RowLayout {
            Layout.fillWidth: true

            QtControls.CheckBox {
                id: animationEnabled

                text: i18n("Show animation when switching:")

                checked: kcm.animationsModel.enabled

                onCheckedChanged: kcm.animationsModel.enabled = checked
            }

            QtControls.ComboBox {
                enabled: animationEnabled.checked

                model: kcm.animationsModel
                textRole: "NameRole"
                currentIndex: kcm.animationsModel.currentIndex
                onActivated: kcm.animationsModel.currentIndex = currentIndex
            }

            QtControls.Button {
                enabled: animationEnabled.checked && kcm.animationsModel.currentConfigurable

                icon.name: "configure"

                onClicked: kcm.configureAnimation()
            }

            QtControls.Button {
                enabled: animationEnabled.checked

                icon.name: "dialog-information"

                onClicked: kcm.showAboutAnimation()
            }

            Item {
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.fillWidth: true

            QtControls.CheckBox {
                id: osdEnabled

                text: i18n("Show on-screen display when switching:")

                checked: kcm.osdEnabled

                onToggled: kcm.osdEnabled = checked
            }

            QtControls.SpinBox {
                id: osdDuration

                enabled: osdEnabled.checked

                from: 0
                to: 10000
                stepSize: 100

                textFromValue: function(value, locale) { return i18n("%1 ms", value)}

                value: kcm.osdDuration

                onValueChanged: kcm.osdDuration = value
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                width: units.largeSpacing
            }

            QtControls.CheckBox {
                id: osdTextOnly

                enabled: osdEnabled.checked

                text: i18n("Show desktop layout indicators")

                checked: !kcm.osdTextOnly

                onToggled: kcm.osdTextOnly = !checked
            }
        }
    }
}

