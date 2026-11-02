/*
    SPDX-FileCopyrightText: 2015 Martin Klapetek <mklapetek@kde.org>
    SPDX-FileCopyrightText: 2019 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
    SPDX-FileCopyrightText: 2026 Mathias Schuh <satalicious@proton.me>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

import org.kde.plasma.kcm.kwinoptions

Item {
    id: root

    property int selectedPosition

    signal positionPicked(int position)

    readonly property int screenWidth: Kirigami.Units.gridUnit * 11
    readonly property int screenHeight: Math.round(screenWidth * Screen.height / Screen.width)

    implicitWidth: screenWidth + Kirigami.Units.smallSpacing * 2
    implicitHeight: screenHeight + Kirigami.Units.smallSpacing * 2

    Kirigami.ShadowedRectangle {
        id: monitorRectangle

        anchors.centerIn: parent
        width: root.screenWidth
        height: root.screenHeight

        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        color: Kirigami.Theme.backgroundColor
        radius: Kirigami.Units.cornerRadius
        shadow.size: Kirigami.Units.gridUnit
        shadow.color: Qt.rgba(0, 0, 0, 0.3)
        shadow.yOffset: 1
    }

    QQC2.ButtonGroup {
        id: positionRadios
    }

    component PositionRadio: QQC2.RadioButton {
        required property int position
        required property string name

        contentItem: Item {}
        checked: root.selectedPosition === position
        onToggled: root.positionPicked(position)

        Accessible.name: name

        QQC2.ToolTip.text: name
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

        QQC2.ButtonGroup.group: positionRadios
    }

    GridLayout {
        anchors.fill: monitorRectangle
        anchors.margins: Kirigami.Units.smallSpacing
        columns: 3
        rowSpacing: 0
        columnSpacing: 0

        Repeater {
            model: [
                {
                    position: KWinOptionsSettings.TopLeft,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Top-left"),
                    alignment: Qt.AlignLeft | Qt.AlignTop,
                },
                {
                    position: KWinOptionsSettings.TopCenter,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Top-center"),
                    alignment: Qt.AlignHCenter | Qt.AlignTop,
                },
                {
                    position: KWinOptionsSettings.TopRight,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Top-right"),
                    alignment: Qt.AlignRight | Qt.AlignTop,
                },
                {
                    position: KWinOptionsSettings.CenterLeft,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Center-left"),
                    alignment: Qt.AlignLeft | Qt.AlignVCenter,
                },
                {
                    position: KWinOptionsSettings.Center,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Center"),
                    alignment: Qt.AlignHCenter | Qt.AlignVCenter,
                },
                {
                    position: KWinOptionsSettings.CenterRight,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Center-right"),
                    alignment: Qt.AlignRight | Qt.AlignVCenter,
                },
                {
                    position: KWinOptionsSettings.BottomLeft,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Bottom-left"),
                    alignment: Qt.AlignLeft | Qt.AlignBottom,
                },
                {
                    position: KWinOptionsSettings.BottomCenter,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Bottom-center"),
                    alignment: Qt.AlignHCenter | Qt.AlignBottom,
                },
                {
                    position: KWinOptionsSettings.BottomRight,
                    name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Bottom-right"),
                    alignment: Qt.AlignRight | Qt.AlignBottom,
                },
            ]

            PositionRadio {
                required property var modelData

                position: modelData.position
                name: modelData.name

                Layout.alignment: modelData.alignment
                Layout.margins: Kirigami.Units.smallSpacing
            }
        }
    }

    // The default position is not on the grid: it sits two thirds down the
    // screen, which is where on-screen displays have traditionally appeared.
    PositionRadio {
        position: KWinOptionsSettings.LowerCenter
        name: i18ndc("kcmkwm", "@option:radio Position of on-screen displays", "Lower center")
        anchors.horizontalCenter: monitorRectangle.horizontalCenter
        y: monitorRectangle.y + Math.round(monitorRectangle.height * 2 / 3 - height / 2)
    }
}
