/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

import org.kde.kwin.kcmtaskswitching

QQC2.ComboBox {

    required property var tabBoxSettings
    readonly property var tabBoxLayouts: kcm.tabBoxLayouts

    readonly property string path: _private.path

    QtObject {
        id: _private
        property string path: ""
    }

    Connections {
        target: tabBoxLayouts
        function onModelReset() { updateModel(); }
    }

    Connections {
        target: tabBoxSettings
        function onLayoutNameChanged() { updateCurrentIndex(); }
        function onShowTabBoxChanged() { updateCurrentIndex(); }
    }

    Component.onCompleted: { updateModel(); }

    model: ListModel {}
    delegate: itemDelegate
    textRole: "name"

    onActivated: changeCurrentIndex()

    function updateModel() {
        model.clear();
        model.append({ "name": i18nc("@item:inlistbox", "None"), "description": "" }); // We must add a 'None' option because we're using a combobox
        for (let i = 0; i < tabBoxLayouts.rowCount(); ++i) {
            let index = tabBoxLayouts.index(i, 0);
            let name = tabBoxLayouts.data(index, TabBoxLayoutsModel.NameRole);
            let description = tabBoxLayouts.data(index, TabBoxLayoutsModel.DescriptionRole);
            model.append({ "name": name, "description": description })
        }

        updateCurrentIndex();
    }

    function changeCurrentIndex() {
        // Make settings changes for user index change
        let chosenModelIndex = currentIndex - 1; // account for extra 'None' item
        if (chosenModelIndex >= 0) {
            // Chosen a layout in our model
            let index = tabBoxLayouts.index(chosenModelIndex, 0);
            tabBoxSettings.showTabBox = true;
            tabBoxSettings.layoutName = tabBoxLayouts.data(index, TabBoxLayoutsModel.PluginIdRole);
        } else {
            // We've chosen 'none'
            tabBoxSettings.showTabBox = false;
        }

        updateLayoutData();
    }

    function updateCurrentIndex() {
        // Update index for settings changes
        if (tabBoxSettings.showTabBox) {
            // Find plugin id in model
            let foundLayout = false;
            for (let i = 0; i < tabBoxLayouts.rowCount(); ++i) {
                let index = tabBoxLayouts.index(i, 0);
                if (tabBoxLayouts.data(index, TabBoxLayoutsModel.PluginIdRole) == tabBoxSettings.layoutName) {
                    currentIndex = i + 1; // Account for 'None'
                    foundLayout = true;
                    break;
                }
            }

            if (!foundLayout) {
                currentIndex = 0; // 'None'
            }
        } else {
            currentIndex = 0; // 'None'
        }

        updateLayoutData();
    }

    function updateLayoutData() {
        let chosenModelIndex = currentIndex - 1;
        if (chosenModelIndex >= 0) {
            let index = tabBoxLayouts.index(chosenModelIndex, 0);
            _private.path = tabBoxLayouts.data(index, TabBoxLayoutsModel.PathRole);
        } else {
            _private.path = "";
        }
    }

    Component {
        id: itemDelegate

        QQC2.ItemDelegate {
            id: delegate

            width: parent?.width ?? 0

            text: model.name

            contentItem: Kirigami.TitleSubtitle {
                title: delegate.text
                subtitle: model.description
                font: delegate.font
                selected: delegate.highlighted || delegate.down
                wrapMode: Text.Wrap
            }
        }
    }
}
