/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.quickcharts as Charts
import org.kde.quickcharts.controls as ChartControls

Rectangle {
    id: root

    required property QtObject effect

    readonly property color gridColor: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                               Kirigami.Theme.backgroundColor.g,
                                               Kirigami.Theme.backgroundColor.b,
                                               0.25)

    color: Qt.rgba(1.0, 1.0, 1.0, 0.5)

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Charts.BarChart {
                id: fpsChart

                Layout.preferredWidth: Kirigami.Units.gridUnit
                Layout.fillHeight: true

                yRange.minimum: root.effect.maximumFps + 10
                yRange.increment: 10

                valueSources: Charts.SingleValueSource { value: root.effect.fps }

                colorSource: Charts.SingleValueSource { value: Kirigami.Theme.highlightColor }

                ChartControls.GridLines {
                    anchors.fill: parent
                    z: -1

                    chart: fpsChart

                    direction: ChartControls.GridLines.Vertical;

                    major.visible: false

                    minor.frequency: 10
                    minor.lineWidth: 1
                    minor.color: root.gridColor
                }
            }

            Charts.LineChart {
                Layout.fillWidth: true
                Layout.fillHeight: true

                yRange.increment: 1000
                yRange.minimum: 5000

                xRange.from: 0
                xRange.automatic: true

                indexingMode: Charts.Chart.IndexSourceValues

                valueSources: [
                    Charts.ModelSource {
                        model: root.effect.paintDurationCPU
                        roleName: "value"
                    },
                    Charts.ModelSource {
                        model: root.effect.paintDuration
                        roleName: "value"
                    },
                ]

                colorSource: Charts.ArraySource {
                    array: ["blue", "red"]
                }

                ChartControls.GridLines {
                    anchors.fill: parent
                    z: -1

                    chart: parent

                    direction: ChartControls.GridLines.Vertical;

                    major.visible: false

                    minor.frequency: 1000
                    minor.lineWidth: 1
                    minor.color: root.gridColor
                }

                Label {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: i18nc("@label", "Paint Duration")
                    font: Kirigami.Theme.smallFont
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            ChartControls.LegendDelegate {
                Layout.fillWidth: true
                Layout.preferredWidth: 0

                name: i18nc("@label", "Current FPS")
                value: root.effect.fps
                color: Kirigami.Theme.highlightColor
            }

            ChartControls.LegendDelegate {
                Layout.fillWidth: true
                Layout.preferredWidth: 0

                name: i18nc("@label", "Maximum FPS")
                value: root.effect.maximumFps
                color: Kirigami.Theme.neutralTextColor
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.effect.presentationMode
        }
    }
}
