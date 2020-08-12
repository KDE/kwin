/*
    KWin - the KDE window manager

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0;

Item {
    id: root

    Loader {
        id: mainItemLoader
    }

    Connections {
        target: workspace
        onCurrentDesktopChanged: {
            if (!mainItemLoader.item) {
                mainItemLoader.source = "osd.qml";
            }
        }
    }
}
