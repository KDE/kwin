/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

import org.kde.kwin.kcmtaskswitching

KCM.AbstractKCM {
    id: root

    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    // TODO: GNS action for Task Switcher (include Task Switcher in string, as multiple pages)

    // TODO: Disambiguation text for alternative task switcher (i.e. at the top, say this
    //       can be used with separate shortcuts to utilise different behaviour)

    ColumnLayout {
        anchors.fill: parent

        spacing: 0

        Kirigami.NavigationTabBar {
            Layout.fillWidth: true

            // TODO: Highlight changed settings?
            //       could be possible with hack and Highlight item
            //       also remember to account for shortcuts

            actions: [
                // TODO: Need custom symbolic icons for each of these, like in
                //       Accessibility & Power Managment KCMs
                Kirigami.Action {
                    text: "Task Switcher" // TODO: i18n
                    icon.name: "preferences-system-tabbox"
                    checked: stackLayout.currentIndex === 0
                    onTriggered: stackLayout.currentIndex = 0
                },
                Kirigami.Action {
                    text: "Task Switcher (Alternative)" // TODO: i18n
                    icon.name: "preferences-system-tabbox"
                    checked: stackLayout.currentIndex === 1
                    onTriggered: stackLayout.currentIndex = 1
                },
                Kirigami.Action {
                    text: "Overview" // TODO: i18n
                    icon.name: "desktop-symbolic"
                    checked: stackLayout.currentIndex === 2
                    onTriggered: stackLayout.currentIndex = 2
                },
                Kirigami.Action {
                    text: "Cube" // TODO: i18n
                    icon.name: "desktop-symbolic"
                    checked: stackLayout.currentIndex === 3
                    onTriggered: stackLayout.currentIndex = 3
                }
            ]
        }

        QQC2.ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true

            //contentWidth: availableWidth - contentItem.leftMargin - contentItem.rightMargin

            Item {
                implicitHeight: stackLayout.implicitHeight + Kirigami.Units.gridUnit * 2
                implicitWidth: scrollView.availableWidth

                StackLayout {
                    id: stackLayout

                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Kirigami.Units.gridUnit

                    currentIndex: 0

                    TabBoxSettings {
                        isAlternative: false
                    }

                    TabBoxSettings {
                        isAlternative: true
                    }

                    Rectangle {
                        color: 'green'
                    }

                    Rectangle {
                        color: 'blue'
                    }
                }
            }
        }
    }
}
