/*
    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.5
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Layouts 1.1

import org.kde.kcm 1.2
import org.kde.kconfig 1.0
import org.kde.kirigami 2.10 as Kirigami
import org.kde.newstuff 1.62 as NewStuff

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
        }
    }

    view: ListView {
        id: effectsList

        property var _buttonGroups: []

        clip: true

        model: Private.EffectsFilterProxyModel {
            id: searchModel

            query: searchField.text
            sourceModel: kcm.effectsModel
        }

        delegate: Effect {
            width: effectsList.width
        }

        section.property: "CategoryRole"
        section.delegate: Kirigami.ListSectionHeader {
            width: effectsList.width
            text: section
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

            NewStuff.Button {
                text: i18n("Get New Desktop Effects...")
                visible: KAuthorized.authorize(KAuthorized.GHNS)
                configFile: "kwineffect.knsrc"
                onEntryEvent: function (entry, event) {
                    if (event == 1) { // StatusChangedEvent
                        kcm.onGHNSEntriesChanged()
                    }
                }
            }
        }
    }
}
