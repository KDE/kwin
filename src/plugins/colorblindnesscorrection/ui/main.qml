/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import org.kde.kcmutils as KCM
import org.kde.plasma.kwin.colorblindnesscorrectioneffect.kcm

KCM.SimpleKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 30
    implicitHeight: Kirigami.Units.gridUnit * 22

    RowLayout {
        id: previewArea
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        Item {
            Layout.fillWidth: true
        }

        Repeater {
            model: [
                { name: i18n("Red"), colors: ["Red", "Orange", "Yellow"] },
                { name: i18n("Green"), colors: ["Green", "LimeGreen", "Lime"] },
                { name: i18n("Blue"), colors: ["Blue", "DeepSkyBlue", "Aqua"] },
                { name: i18n("Purple"), colors: ["Purple", "Fuchsia", "Violet"] },
            ]

            delegate: Column {
                spacing: 0

                Repeater {
                    model: modelData.colors
                    delegate: Rectangle {
                        width: Kirigami.Units.gridUnit * 5
                        height: Kirigami.Units.gridUnit * 5
                        color: modelData
                    }
                }

                QQC.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.name
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }

    Kirigami.FormLayout {
        id: formLayout
        anchors {
            top: previewArea.bottom
            topMargin: Kirigami.Units.largeSpacing
        }

        QQC.ComboBox {
            Kirigami.FormData.label: i18nc("@label", "Mode:")
            currentIndex: kcm.settings.mode
            textRole: "text"
            valueRole: "value"
            model: [
                { value: 0, text: i18nc("@option", "Protanopia (red weak)") },
                { value: 1, text: i18nc("@option", "Deuteranopia (green weak)") },
                { value: 2, text: i18nc("@option", "Tritanopia (blue-yellow)") },
            ]

            onActivated: kcm.settings.mode = currentValue
        }
    }
}
