/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012, 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import org.kde.kirigami.platform as KirigamiPlatform
import org.kde.kwin

PlasmaCore.Window {
    id: window
    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    visible: true
    opacity: 0.5

    required property var screen
    property rect screenGeometry: Workspace.clientArea(KWin.MaximizeArea, screen, Workspace.currentDesktopForScreen(screen));
    property bool atBottom: false

    readonly property int widthWithPadding: dialogItem.implicitWidth + leftPadding + rightPadding
    readonly property int heightWithPadding: dialogItem.implicitHeight + topPadding + bottomPadding

    width: widthWithPadding
    height: heightWithPadding
    x: screenGeometry.x + screenGeometry.width - widthWithPadding - 6 * KirigamiPlatform.Units.largeSpacing
    y: screenGeometry.y + (atBottom ? screenGeometry.height - heightWithPadding - 6 * KirigamiPlatform.Units.largeSpacing
                                    : 6 * KirigamiPlatform.Units.largeSpacing)

    mainItem: Item {
        id: dialogItem

        implicitWidth: Math.ceil(textElements.implicitWidth) + 2 * KirigamiPlatform.Units.largeSpacing
        implicitHeight: textElements.implicitHeight + 2 * KirigamiPlatform.Units.largeSpacing

        ColumnLayout {
            id: textElements
            anchors.fill: parent

            spacing: 0

            Kirigami.Heading {
                text: i18nc("@title OSD text to remind the user that they are in a Safe Mode session", "Safe Mode")
                Layout.alignment: Qt.AlignHCenter
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }

            PlasmaComponents.Label {
                text: i18nc("@info OSD subtitle to remind the user that they are in a Safe Mode session", "Desktop and application settings are temporary.<br>Any changes will be lost after logout.")
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: { window.atBottom = !window.atBottom; }
        }
    }
}
