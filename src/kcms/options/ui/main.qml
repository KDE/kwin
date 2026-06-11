/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

import org.kde.plasma.kcm.kwinoptions

KCM.SimpleKCM {
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    ColumnLayout {
        spacing: 0

        Kirigami.NavigationTabBar {
            Kirigami.Theme.colorSet: Kirigami.Theme.Window
            Layout.fillWidth: true

            actions: [
                Kirigami.Action {
                    icon.name: "window-focus"
                    text: i18nd("kcmkwm", "&Focus")
                    checked: stackLayout.currentIndex === 0
                    onTriggered: stackLayout.currentIndex = 0
                },
                Kirigami.Action {
                    icon.name: "system-run-symbolic"
                    text: i18nd("kcmkwm", "Titlebar A&ctions")
                    checked: stackLayout.currentIndex === 1
                    onTriggered: stackLayout.currentIndex = 1
                },
                Kirigami.Action {
                    icon.name: "system-run-symbolic"
                    text: i18nd("kcmkwm", "W&indow Actions")
                    checked: stackLayout.currentIndex === 2
                    onTriggered: stackLayout.currentIndex = 2
                },
                Kirigami.Action {
                    icon.name: "preferences-system-windows-move-symbolic"
                    text: i18nd("kcmkwm", "Mo&vement & Placement")
                    checked: stackLayout.currentIndex === 3
                    onTriggered: stackLayout.currentIndex = 3
                },
            ]
        }

        StackLayout {
            id: stackLayout

            Layout.preferredHeight: children[currentIndex].implicitHeight

            Focus {}
            TitlebarActions {}
            WindowActions {}
            MovementAndPlacement {}
        }
    }
}
