/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

QQC2.ComboBox {
    id: root

    required property var tabBoxSettings

    model: kcm.tabBoxLayouts
    delegate: Kirigami.SubtitleDelegate {
        required property int index
        required property string name
        required property string description

        width: ListView.view?.width ?? implicitWidth

        // Don't reserve space for an icon
        icon.width: 0
        icon.height: 0

        text: name
        subtitle: description
        highlighted: root.highlightedIndex === index
    }
    textRole: "name"
    valueRole: "pluginId"

    currentIndex: {
        const pluginId = tabBoxSettings.showTabBox ? tabBoxSettings.layoutName : "";
        return count > 0 ? Math.max(indexOfValue(pluginId), 0) : -1;
    }

    onActivated: {
        tabBoxSettings.layoutName = currentValue;
        tabBoxSettings.showTabBox = currentValue !== "";
    }
}
