/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

Item {
    id: root

    property QtObject effectFrame: null

    implicitWidth: layout.implicitWidth + layout.anchors.leftMargin + layout.anchors.rightMargin
    implicitHeight: layout.implicitHeight + layout.anchors.topMargin + layout.anchors.bottomMargin

    PlasmaCore.FrameSvgItem {
        id: frameSvg
        imagePath: "widgets/background"
        opacity: root.effectFrame.frameOpacity
        anchors.fill: parent
    }

    RowLayout {
        id: layout
        anchors {
            fill: parent
            leftMargin: frameSvg.fixedMargins.left
            rightMargin: frameSvg.fixedMargins.right
            topMargin: frameSvg.fixedMargins.top
            bottomMargin: frameSvg.fixedMargins.bottom
        }
        spacing: PlasmaCore.Units.smallSpacing

        PlasmaCore.IconItem {
            id: icon
            Layout.preferredWidth: root.effectFrame.iconSize.width
            Layout.preferredHeight: root.effectFrame.iconSize.height
            Layout.alignment: Qt.AlignHCenter
            animated: root.effectFrame.crossFadeEnabled
            visible: valid
            source: root.effectFrame.icon
        }

        PlasmaComponents.Label {
            id: label
            Layout.fillWidth: true
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font: root.effectFrame.font
            visible: text !== ""
            text: root.effectFrame.text
        }
    }
}
