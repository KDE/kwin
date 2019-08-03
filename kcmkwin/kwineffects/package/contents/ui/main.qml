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
 */

import QtQuick 2.5
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Layouts 1.1

import org.kde.kcm 1.2
import org.kde.kconfig 1.0
import org.kde.kirigami 2.8 as Kirigami
import org.kde.private.kcms.kwin.effects 1.0 as Private

ScrollViewKCM {
    ConfigModule.quickHelp: i18n("This module lets you configure desktop effects.")

    header: ColumnLayout {
        QQC2.Label {
            Layout.fillWidth: true

            elide: Text.ElideRight
            text: i18n("Hint: To find out or configure how to activate an effect, look at the effect's settings.")
        }

        RowLayout {
            Kirigami.SearchField {
                id: searchField

                Layout.fillWidth: true
            }

            QQC2.ToolButton {
                id: filterButton

                icon.name: "view-filter"

                checkable: true
                checked: menu.opened
                onClicked: menu.popup(filterButton, filterButton.width - menu.width, filterButton.height)

                QQC2.ToolTip {
                    text: i18n("Configure Filter")
                }
            }

            QQC2.Menu {
                id: menu

                modal: true

                QQC2.MenuItem {
                    checkable: true
                    checked: searchModel.excludeUnsupported
                    text: i18n("Exclude unsupported effects")

                    onToggled: searchModel.excludeUnsupported = checked
                }

                QQC2.MenuItem {
                    checkable: true
                    checked: searchModel.excludeInternal
                    text: i18n("Exclude internal effects")

                    onToggled: searchModel.excludeInternal = checked
                }
            }
        }
    }

    view: ListView {
        id: effectsList

        property var _buttonGroups: []

        model: Private.EffectsFilterProxyModel {
            id: searchModel

            query: searchField.text
            sourceModel: kcm.effectsModel
        }

        delegate: Effect {
            width: effectsList.width
        }

        section.property: "CategoryRole"
        section.delegate:Kirigami.AbstractListItem {
            width: effectsList.width

            backgroundColor: Kirigami.Theme.backgroundColor
            Kirigami.Theme.inherit: false
            Kirigami.Theme.colorSet: Kirigami.Theme.Window

            hoverEnabled: false
            supportsMouseEvents: false

            Kirigami.Heading {
                level: 3
                text: section
            }
        }

        function findButtonGroup(name) {
            for (let item of effectsList._buttonGroups) {
                if (item.name == name) {
                    return item.group;
                }
            }

            let group = Qt.createQmlObject(
                'import QtQuick 2.5;' +
                'import QtQuick.Controls 2.5;' +
                'ButtonGroup {}',
                effectsList,
                "dynamicButtonGroup" + effectsList._buttonGroups.length
            );

            effectsList._buttonGroups.push({ name, group });

            return group;
        }
    }

    footer: ColumnLayout {
        RowLayout {
            Layout.alignment: Qt.AlignRight

            QQC2.Button {
                icon.name: "get-hot-new-stuff"
                text: i18n("Get New Desktop Effects...")
                visible: KAuthorized.authorize("ghns")

                onClicked: kcm.openGHNS(this)
            }
        }
    }
}
