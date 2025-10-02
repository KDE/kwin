/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.extras as PlasmaExtras
import QtQuick.Controls as QQC2

Item {
    id: container

    readonly property QtObject effect: KWinComponents.SceneView.effect
    readonly property QtObject targetScreen: KWinComponents.SceneView.screen

    Component.onCompleted: console.info(container.effect.leftButtons)

    Rectangle {
        anchors.fill: parent

        // Top-side buttons
        RowLayout {
            anchors {
                top: parent.top
                topMargin: Kirigami.Units.largeSpacing
                horizontalCenter: parent.horizontalCenter
            }

            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: 5

                delegate: ColumnLayout {
                    id: buttonLayout

                    required property int index

                    Rectangle {
                        width: 75
                        height: 40

                        color: "red"
                    }

                    QQC2.Label {
                        text: "Button " + buttonLayout.index
                        horizontalAlignment: Text.AlignHCenter

                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Bottom-side buttons
        RowLayout {
            anchors {
                bottom: parent.bottom
                bottomMargin: Kirigami.Units.largeSpacing
                horizontalCenter: parent.horizontalCenter
            }

            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: 5

                delegate: ColumnLayout {
                    id: buttonLayout

                    required property int index

                    QQC2.Label {
                        text: "Button " + buttonLayout.index
                        horizontalAlignment: Text.AlignHCenter

                        Layout.fillWidth: true
                    }

                    Rectangle {
                        width: 75
                        height: 40

                        color: "red"
                    }
                }
            }
        }

        // Left-side buttons
        ColumnLayout {
            anchors {
                left: parent.left
                leftMargin: Kirigami.Units.largeSpacing
                verticalCenter: parent.verticalCenter
            }

            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: container.effect.leftButtons

                delegate: RowLayout {
                    id: buttonLayout

                    required property int index

                    Rectangle {
                        width: 75
                        height: 40

                        color: "red"
                    }

                    QQC2.Label {
                        text: "Button " + buttonLayout.index
                    }
                }
            }
        }

        // Right-side buttons
        ColumnLayout {
            anchors {
                right: parent.right
                rightMargin: Kirigami.Units.largeSpacing
                verticalCenter: parent.verticalCenter
            }

            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: 5

                delegate: RowLayout {
                    id: buttonLayout

                    required property int index

                    QQC2.Label {
                        text: "Button " + buttonLayout.index
                    }

                    Rectangle {
                        width: 75
                        height: 40

                        color: "red"
                    }
                }
            }
        }

        color: Kirigami.Theme.backgroundColor
        opacity: 0.75
    }
}
