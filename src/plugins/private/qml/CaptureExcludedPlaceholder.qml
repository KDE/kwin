/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents

Rectangle {
    id: placeholder

    required property KWinComponents.Window window
    readonly property bool active: KWinComponents.Workspace.screenCaptureActive && (placeholder.window?.excludeFromCapture ?? false)

    visible: placeholder.active
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.backgroundColor

    Kirigami.Icon {
        anchors.centerIn: parent
        width: Math.min(Kirigami.Units.iconSizes.enormous, parent.width / 2, parent.height / 2)
        height: width
        source: placeholder.window?.icon ?? ""
    }
}
