/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import org.kde.kwin

Item {
    id: root

    Loader {
        id: mainItemLoader
    }

    Connections {
        target: Workspace
        function onCurrentDesktopChanged() {
            if (!mainItemLoader.item) {
                mainItemLoader.source = "osd.qml";
            }
        }
    }
}
