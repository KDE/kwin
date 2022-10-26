/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Window 2.15
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kwin 3.0 as KWinComponents

Window {
    id: root

    required property QtObject handle

    visible: false
    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    color: Qt.rgba(PlasmaCore.ColorScope.backgroundColor.r,
                   PlasmaCore.ColorScope.backgroundColor.g,
                   PlasmaCore.ColorScope.backgroundColor.b, 0.25)

    Rectangle {
        anchors.centerIn: parent
        width: handle.frameGeometry.width
        height: handle.frameGeometry.height
        radius: 3
        color: PlasmaCore.ColorScope.highlightColor
        opacity: 0.25
    }

    function place() {
        const area = KWinComponents.Workspace.clientArea(KWinComponents.Workspace.MaximizeArea, handle);
        x = area.x;
        y = area.y;
        width = area.width;
        height = area.height;
    }

    Connections {
        target: handle
        function onOutputChanged() {
            place();
        }
    }

    Component.onCompleted: {
        place();
        KWin.registerWindow(root);
        visible = true;
    }
}
