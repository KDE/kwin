/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012, 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Window as QuickWindow
import QtQuick.Layouts
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import org.kde.kwin

QuickWindow.Window {
    id: window
    flags: Qt.BypassWindowManagerHint | Qt.SplashScreen | Qt.FramelessWindowHint | /*Qt.WindowStaysOnTopHint | */Qt.WindowDoesNotAcceptFocus
    color: "transparent"

    required property var screen
    property rect screenGeometry: Workspace.clientArea(KWin.MaximizeArea, screen, Workspace.currentDesktopForScreen(screen));
    property bool atBottom: false

    width: dialogItem.implicitWidth
    height: dialogItem.implicitHeight
    x: screenGeometry.x + screenGeometry.width - dialogItem.implicitWidth - 6 * Kirigami.Units.largeSpacing
    y: screenGeometry.y + screenGeometry.height - dialogItem.implicitHeight - 6 * Kirigami.Units.largeSpacing

    Component.onCompleted: { window.show(); }

    Rectangle {
        id: dialogItem

        color: "transparent"

        implicitWidth: Math.ceil(textElements.implicitWidth) + 2 * Kirigami.Units.largeSpacing
        implicitHeight: textElements.implicitHeight + 2 * Kirigami.Units.largeSpacing

        ColumnLayout {
            id: textElements
            anchors.fill: parent

            spacing: 0

            PlasmaExtras.ShadowedLabel {
                id: title
                text: i18nc("@title OSD text to remind the user that they are in a Safe Mode session", "Safe Mode")
                // Emulate the size of a level 1 heading
                font.pointSize: Math.round(Kirigami.Theme.defaultFont.pointSize * 1.35)

                Layout.alignment: Qt.AlignHCenter
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }

            PlasmaExtras.ShadowedLabel {
                id: subtitle
                text: i18nc("@info OSD subtitle to remind the user that they are in a Safe Mode session", "Desktop and application settings are temporary.\nAny changes will be lost after logout.")
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
        }

        DragHandler {
            target: null
            cursorShape: Qt.DragMoveCursor
            onActiveChanged: {
                if (active) {
                    window.startSystemMove();
                }
            }
        }
    }
}
