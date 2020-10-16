/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import org.kde.kwin.decoration 0.1

Item {
    property QtObject borders: Borders {
        objectName: "borders"
    }
    property QtObject maximizedBorders: Borders {
        objectName: "maximizedBorders"
    }
    property QtObject extendedBorders: Borders {
        objectName: "extendedBorders"
    }
    property QtObject padding: Borders {
        objectName: "padding"
    }
    property bool alpha: true
    width: decoration.client.width + decoration.borderLeft + decoration.borderRight + (decoration.client.maximized ? 0 : (padding.left + padding.right))
    height: (decoration.client.shaded ? 0 : decoration.client.height) + decoration.borderTop + (decoration.client.shaded ? 0 : decoration.borderBottom) + (decoration.client.maximized ? 0 : (padding.top + padding.bottom))
}
