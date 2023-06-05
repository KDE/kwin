/*
    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcm
import org.kde.config
import org.kde.kirigami 2.10 as Kirigami
import org.kde.newstuff as NewStuff

import org.kde.private.kcms.kwin.effects as Private

ScrollViewKCM {
    implicitHeight: Kirigami.Units.gridUnit * 30
    implicitWidth: Kirigami.Units.gridUnit * 40

    actions: NewStuff.Action {
        text: i18nc("@action:button get new KWin effects", "Get New…")
        visible: KAuthorized.authorize(KAuthorized.GHNS)
        configFile: "kwineffect.knsrc"
        onEntryEvent: (entry, event) => {
            if (event === NewStuff.Engine.StatusChangedEvent) {
                kcm.onGHNSEntriesChanged()
            }
        }
    }

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing

            wrapMode: Text.WordWrap
            text: i18n("Hint: To find out or configure how to activate an effect, look at the effect's settings.")
        }

        RowLayout {
            spacing: Kirigami.Units.smallSpacing

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

        // { string name: QQC2.ButtonGroup group }
        property var _buttonGroups: new Map()

        clip: true

        model: Private.EffectsFilterProxyModel {
            id: searchModel

            query: searchField.text
            sourceModel: kcm.effectsModel
        }

        delegate: Effect {
            width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        }

        section.property: "CategoryRole"
        section.delegate: Kirigami.ListSectionHeader {
            width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
            text: section
        }

        Component {
            id: buttonGroupComponent

            QQC2.ButtonGroup {}
        }

        function findButtonGroup(name: string): QQC2.ButtonGroup {
            let group = _buttonGroups.get(name);
            if (group === undefined) {
                group = buttonGroupComponent.createObject(this);
                _buttonGroups.set(name, group);
            }
            return group;
        }
    }
}
