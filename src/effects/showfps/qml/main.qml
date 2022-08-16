/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import org.kde.kirigami 2.18 as Kirigami
import org.kde.quickcharts 1.0 as Charts
import org.kde.quickcharts.controls 1.0 as ChartControls

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

                Charts.GridLines {
                    anchors.fill: parent
                    z: -1

                    chart: fpsChart

                    direction: Charts.GridLines.Vertical;

                    major.visible: false

                    minor.frequency: 10
                    minor.lineWidth: 1
                    minor.color: root.gridColor
                }
            }

            Charts.BarChart {
                Layout.fillWidth: true
                Layout.fillHeight: true

                yRange.minimum: 100
                yRange.increment: 10

                xRange.from: 0
                xRange.to: 50
                xRange.automatic: false

                indexingMode: Charts.Chart.IndexSourceValues

                valueSources: Charts.HistoryProxySource {
                    source: Charts.SingleValueSource {
                        value: root.effect.paintDuration
                    }
                    maximumHistory: 100
                    fillMode: Charts.HistoryProxySource.FillFromStart
                }

                colorSource: Charts.HistoryProxySource {
                    source: Charts.SingleValueSource {
                        value: root.effect.paintColor
                    }
                    maximumHistory: 100
                    fillMode: Charts.HistoryProxySource.FillFromStart
                }

                Charts.GridLines {
                    anchors.fill: parent
                    z: -1

                    chart: parent

                    direction: Charts.GridLines.Vertical;

                    major.visible: false

                    minor.frequency: 10
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

            Charts.BarChart {
                Layout.fillWidth: true
                Layout.fillHeight: true

                yRange.minimum: Qt.application.screens[0].width * Qt.application.screens[0].height
                yRange.increment: 500000

                xRange.from: 0
                xRange.to: 50
                xRange.automatic: false

                indexingMode: Charts.Chart.IndexSourceValues

                valueSources: Charts.HistoryProxySource {
                    source: Charts.SingleValueSource {
                        value: root.effect.paintAmount
                    }
                    maximumHistory: 100
                    fillMode: Charts.HistoryProxySource.FillFromStart
                }

                colorSource: Charts.HistoryProxySource {
                    source: Charts.SingleValueSource {
                        value: root.effect.paintColor
                    }
                    maximumHistory: 100
                    fillMode: Charts.HistoryProxySource.FillFromStart
                }

                Charts.GridLines {
                    anchors.fill: parent
                    z: -1

                    chart: parent

                    direction: Charts.GridLines.Vertical;

                    major.visible: false

                    minor.frequency: 100000
                    minor.lineWidth: 1
                    minor.color: root.gridColor
                }

                Label {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: i18nc("@label", "Paint Amount")
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
            text: i18nc("@label", "This effect is not a benchmark")
        }
    }
}
