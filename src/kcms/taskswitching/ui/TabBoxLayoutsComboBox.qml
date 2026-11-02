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
    delegate: itemDelegate
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
