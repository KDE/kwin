/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

import org.kde.plasma.kcm.animations

QQC2.ComboBox {
    id: animationComboBox

    required property var animationsModel

    readonly property bool isDefault: _private.isDefault
    readonly property bool isConfigurable: _private.isConfigurable
    readonly property bool isAnyConfigurable: _private.isAnyConfigurable
    readonly property string configurePluginId: _private.configurePluginId

    QtObject {
        id: _private
        property bool isDefault: true
        property bool isConfigurable: true
        property bool isAnyConfigurable: true
        property string configurePluginId: ""
    }

    Connections {
        target: animationsModel
        function onModelReset() { updateModel(); }
        function onDataChanged() { updateCurrentIndex(); }
    }

    Component.onCompleted: { updateModel(); }

    model: ListModel {}
    delegate: itemDelegate
    textRole: "name"

    onActivated: changeCurrentIndex()

    function updateModel() {
        model.clear();
        model.append({ "name": "None", "description": "" }); // We must add a 'None' option because we're using a combobox
        for (let i = 0; i < animationsModel.rowCount(); ++i) {
            let index = animationsModel.index(i, 0);
            let name = animationsModel.data(index, EffectsModel.NameRole);
            let description = animationsModel.data(index, EffectsModel.DescriptionRole);
            model.append({ "name": name, "description": description })
        }

        updateCurrentIndex();
    }

    function changeCurrentIndex() {
        // Make model changes for user index change
        let chosenModelIndex = currentIndex - 1; // account for extra 'None' item
        if (chosenModelIndex >= 0) {
            // It's in our model, enable it
            let index = animationsModel.index(chosenModelIndex, 0);
            animationsModel.setData(index, Qt.Checked, EffectsModel.StatusRole);
        } else {
            // We've chosen 'none', disable enabled animation
            for (let i = 0; i < animationsModel.rowCount(); ++i) {
                let index = animationsModel.index(i, 0);
                if (animationsModel.data(index, EffectsModel.StatusRole) == Qt.Checked) {
                    animationsModel.setData(index, Qt.Unchecked, EffectsModel.StatusRole);
                    return;
                }
            }
        }

        // NOTE: We don't have to call updateEffectData() because our changes will cause
        // dataChanged() emit by animationsModel and go to updateCurrentIndex
    }

    function updateCurrentIndex() {
        // Update index for model changes
        for (let i = 0; i < animationsModel.rowCount(); ++i) {
            let index = animationsModel.index(i, 0);
            if (animationsModel.data(index, EffectsModel.StatusRole) == Qt.Checked) {
                currentIndex = i + 1; // account for extra 'None' item
                updateEffectData();
                return;
            }
        }

        currentIndex = 0; // 'None'
        updateEffectData();
    }

    function updateEffectData() {
        let checkIsDefault = true;
        for (let i = 0; i < animationsModel.rowCount(); ++i) {
            let index = animationsModel.index(i, 0);
            let enabled = (animationsModel.data(index, EffectsModel.StatusRole) == Qt.Checked);
            let enabledByDefault = animationsModel.data(index, EffectsModel.EnabledByDefaultRole);
            if (enabled != enabledByDefault) {
                checkIsDefault = false;
                break;
            }
        }

        let checkIsConfigurable = true;
        let checkConfigurePluginId = "";
        let chosenModelIndex = currentIndex - 1; // account for extra 'None' item
        if (chosenModelIndex >= 0) {
            // It's in our model
            let index = animationsModel.index(chosenModelIndex, 0);
            checkIsConfigurable = animationsModel.data(index, EffectsModel.ConfigurableRole);
            checkConfigurePluginId = animationsModel.data(index, EffectsModel.ServiceNameRole);
        } else {
            // 'None' is not in our model
            checkIsConfigurable = false;
        }

        let checkIsAnyConfigurable = false;
        for (let i = 0; i < animationsModel.rowCount(); ++i) {
            let index = animationsModel.index(i, 0);
            if (animationsModel.data(index, EffectsModel.ConfigurableRole)) {
                checkIsAnyConfigurable = true;
            }
        }

        _private.isDefault = checkIsDefault;
        _private.isConfigurable = checkIsConfigurable;
        _private.isAnyConfigurable = checkIsAnyConfigurable;
        _private.configurePluginId = checkConfigurePluginId;
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
