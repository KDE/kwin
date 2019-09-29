/*
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
 * Copyright (C) 2019 Vlad Zahorodnii <vladzzag@gmail.com>
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

import QtQuick 2.5
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Layouts 1.1

import org.kde.kirigami 2.5 as Kirigami

Kirigami.SwipeListItem {
    id: listItem
    hoverEnabled: true
    onClicked: {
        view.currentIndex = index;
    }
    contentItem: RowLayout {
        id: row
        QQC2.RadioButton {
            property bool _exclusive: model.ExclusiveRole != ""
            property bool _toggled: false

            checked: model.StatusRole
            visible: _exclusive
            QQC2.ButtonGroup.group: _exclusive ? effectsList.findButtonGroup(model.ExclusiveRole) : null

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

        QQC2.CheckBox {
            checkState: model.StatusRole
            visible: model.ExclusiveRole == ""

            onToggled: model.StatusRole = checkState
        }

        ColumnLayout {
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            spacing: 0

            Kirigami.Heading {
                Layout.fillWidth: true

                level: 5
                text: model.NameRole
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                Layout.fillWidth: true

                text: model.DescriptionRole
                opacity: listItem.hovered ? 0.8 : 0.6
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                id: aboutItem

                Layout.fillWidth: true

                text: i18n("Author: %1\nLicense: %2", model.AuthorNameRole, model.LicenseRole)
                opacity: listItem.hovered ? 0.8 : 0.6
                visible: view.currentIndex === index
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
    }
    actions: [
        Kirigami.Action {
            visible: model.VideoRole.toString() !== ""
            icon.name: "videoclip-amarok"
            tooltip: i18nc("@info:tooltip", "Show/Hide Video")
            onTriggered: videoItem.showHide()
        },
        Kirigami.Action {
            visible: model.ConfigurableRole
            enabled: model.StatusRole != Qt.Unchecked
            icon.name: "configure"
            tooltip: i18nc("@info:tooltip", "Configure...")
            onTriggered: kcm.configure(model.ServiceNameRole, this)
        }
    ]
}
