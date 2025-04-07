/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.plasma.kcm.animations

QQC2.CheckBox {
    id: animationCheckBox

    required property var animationsModel
    required property int index

    readonly property bool isDefault: _private.isDefault
    readonly property bool isConfigurable: _private.isConfigurable
    readonly property string configurePluginId: _private.configurePluginId

    QtObject {
        id: _private
        property bool isDefault: true
        property bool isConfigurable: true
        property string configurePluginId: ""
    }

    Connections {
        target: animationsModel
        function onModelReset() { update(); }
        function onDataChanged() { update(); }
    }

    Component.onCompleted: {
        let index = animationsModel.index(animationCheckBox.index, 0);

        _private.isConfigurable = animationsModel.data(index, EffectsModel.ConfigurableRole);
        _private.configurePluginId = animationsModel.data(index, EffectsModel.ServiceNameRole);

        update();
    }

    function update() {
        let index = animationsModel.index(animationCheckBox.index, 0);

        let enabled = (animationsModel.data(index, EffectsModel.StatusRole) == Qt.Checked);
        let enabledByDefault = animationsModel.data(index, EffectsModel.EnabledByDefaultRole);

        animationCheckBox.checked = enabled;
        _private.isDefault = (enabled == enabledByDefault);
    }

    onToggled: {
        let index = animationsModel.index(animationCheckBox.index, 0);
        animationsModel.setData(index, (animationCheckBox.checked ? Qt.Checked : Qt.Unchecked), EffectsModel.StatusRole);
        update();
    }
}
