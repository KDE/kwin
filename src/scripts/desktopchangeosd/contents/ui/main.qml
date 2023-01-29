/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0;
import org.kde.kwin 3.0

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
