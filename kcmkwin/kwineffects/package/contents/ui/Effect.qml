/*
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
 * Copyright (C) 2019 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

import QtQuick 2.1
import QtQuick.Controls 2.5 as QtControls
import QtQuick.Layouts 1.0
import org.kde.kirigami 2.5 as Kirigami

Rectangle {
    height: row.implicitHeight

    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    color: index % 2 ? Kirigami.Theme.backgroundColor : palette.alternateBase

    RowLayout {
        id: row

        x: spacing
        width: parent.width - 2 * spacing

        QtControls.RadioButton {
            property bool _exclusive: model.ExclusiveRole != ""
            property bool _toggled: false

            checked: model.StatusRole
            visible: _exclusive
            QtControls.ButtonGroup.group: _exclusive ? effectsList.findButtonGroup(model.ExclusiveRole) : null

            onToggled: {
                model.StatusRole = checked ? Qt.Checked : Qt.Unchecked;
                _toggled = true;
            }
            onClicked: {
                // Uncheck the radio button if it's clicked.
                if (checked && !_toggled) {
                    model.StatusRole = Qt.Unchecked;
                }
                _toggled = false;
            }
        }

        QtControls.CheckBox {
            checkState: model.StatusRole
            visible: model.ExclusiveRole == ""

            onToggled: model.StatusRole = checkState
        }

        ColumnLayout {
            QtControls.Label {
                Layout.fillWidth: true

                font.weight: Font.Bold
                text: model.NameRole
                wrapMode: Text.Wrap
            }

            QtControls.Label {
                Layout.fillWidth: true

                text: model.DescriptionRole
                wrapMode: Text.Wrap
            }

            QtControls.Label {
                id: aboutItem

                Layout.fillWidth: true

                font.weight: Font.Bold
                text: i18n("Author: %1\nLicense: %2", model.AuthorNameRole, model.LicenseRole)
                visible: false
                wrapMode: Text.Wrap
            }

            Loader {
                id: videoItem

                active: false
                source: "Video.qml"
                visible: false

                function showHide() {
                    if (!videoItem.active) {
                        videoItem.active = true;
                        videoItem.visible = true;
                    } else {
                        videoItem.active = false;
                        videoItem.visible = false;
                    }
                }
            }
        }

        QtControls.Button {
            icon.name: "video"
            visible: model.VideoRole.toString() !== ""

            onClicked: videoItem.showHide()
        }

        QtControls.Button {
            enabled: model.StatusRole != Qt.Unchecked
            icon.name: "configure"
            visible: model.ConfigurableRole

            onClicked: kcm.configure(model.ServiceNameRole, this)
        }

        QtControls.Button {
            icon.name: "dialog-information"

            onClicked: aboutItem.visible = !aboutItem.visible;
        }
    }

    // Kirigami.Theme doesn't provide alternate color.
    SystemPalette {
        id: palette

        colorGroup: SystemPalette.Active
    }
}
