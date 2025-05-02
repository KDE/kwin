/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.newstuff as NewStuff

import org.kde.kwin.kcmtaskswitching

KCM.AbstractKCM {
    id: root

    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    actions: [
        NewStuff.Action {
            id: newStuffButton
            text: i18n("&Get New Task Switcher Styles")
            configFile: "kwinswitcher.knsrc"
            viewMode: NewStuff.Page.ViewMode.Tiles
            onEntryEvent: (entry, event) => {
                if (event === NewStuff.Entry.StatusChangedEvent) {
                    kcm.ghnsEntryChanged();
                }
            }
        }
    ]

    ColumnLayout {
        anchors.fill: parent

        spacing: 0

        Kirigami.NavigationTabBar {
            Layout.fillWidth: true

            actions: [
                // TODO: Need custom symbolic icons for each of these, like in
                //       Accessibility & Power Managment KCMs
                Kirigami.Action {
                    text: i18nc("@title:tab", "Task Switcher")
                    icon.name: "preferences-system-tabbox"
                    checked: stackLayout.currentIndex === 0
                    onTriggered: stackLayout.currentIndex = 0
                },
                Kirigami.Action {
                    text: i18nc("@title:tab", "Task Switcher (Alternative)")
                    icon.name: "preferences-system-tabbox"
                    checked: stackLayout.currentIndex === 1
                    onTriggered: stackLayout.currentIndex = 1
                },
                Kirigami.Action {
                    text: i18nc("@title:tab", "Overview")
                    icon.name: "desktop-symbolic"
                    checked: stackLayout.currentIndex === 2
                    onTriggered: stackLayout.currentIndex = 2
                }
            ]
        }

        QQC2.ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                width: scrollView.availableWidth
                height: Math.max(implicitHeight, scrollView.availableHeight)
                // NOTE: No need to calculate implicitWidth, as we don't use it for sizing and
                //       if present, the ScrollView will use it to show horizontal scroll bars
                //implicitWidth: stackLayout.implicitWidth + Kirigami.Units.gridUnit * 2
                implicitHeight: stackLayout.implicitHeight + Kirigami.Units.gridUnit * 2

                StackLayout {
                    id: stackLayout
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.gridUnit

                    currentIndex: 0

                    // "Task Switcher"
                    TabBoxSettings {
                        isAlternative: false
                    }

                    // "Task Switcher (Alternative)"
                    ColumnLayout {
                        spacing: Kirigami.Units.largeSpacing

                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                            Layout.alignment: Qt.AlignHCenter

                            horizontalAlignment: Text.AlignHCenter
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap

                            text: i18n("The alternative task switcher is a second switcher with its own separate shortcut, appearance, and behavior settings.")
                        }

                        TabBoxSettings {
                            isAlternative: true
                        }

                    }

                    // "Overview"
                    OverviewSettings {}
                }
            }
        }
    }
}
