/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2

import org.kde.kirigami 2.12 as Kirigami

ColumnLayout {
    id: root

    property QtObject effectFrame: null

    spacing: 5

    Kirigami.Icon {
        id: icon
        Layout.preferredWidth: root.effectFrame.iconSize.width
        Layout.preferredHeight: root.effectFrame.iconSize.height
        Layout.alignment: Qt.AlignHCenter
        visible: valid
        source: root.effectFrame.icon
    }

    QQC2.Label {
        id: label
        Layout.fillWidth: true
        textFormat: Text.PlainText
        elide: Text.ElideRight
        font: root.effectFrame.font
        visible: text !== ""
        text: root.effectFrame.text
    }
}
