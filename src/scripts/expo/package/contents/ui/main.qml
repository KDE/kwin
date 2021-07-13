/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Window 2.12
import org.kde.kwin 2.0 as KWinComponents
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: root

    property bool activated: false

    function toggleActivate() {
        root.activated = !root.activated;
        if (root.activated) {
            expoLoader.active = true;
        } else {
            if (expoLoader.item) {
                shutdownTimer.start();
                expoLoader.item.stop();
            }
        }
    }

    Loader {
        id: expoLoader
        active: false
        sourceComponent: Expo {}
    }

    Timer {
        id: shutdownTimer
        interval: PlasmaCore.Units.longDuration
        running: false
        repeat: false
        onTriggered: expoLoader.active = false
    }

    KWinComponents.ScreenEdgeItem {
        edge: KWinComponents.ScreenEdgeItem.TopLeftEdge
        mode: KWinComponents.ScreenEdgeItem.Pointer
        onActivated: toggleActivate()
    }

    KWinComponents.ScreenEdgeItem {
        edge: KWinComponents.ScreenEdgeItem.TopLeftEdge
        mode: KWinComponents.ScreenEdgeItem.Touch
        onActivated: toggleActivate
    }

    Component.onCompleted: {
        KWin.registerShortcut("Expo", "Expo", "Ctrl+Meta+D", () => toggleActivate());
    }
}
