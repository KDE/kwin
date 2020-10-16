/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import QtQuick.Layouts 1.2
import org.kde.plasma.components 3.0 as Plasma

RowLayout {
    Plasma.Button {
        objectName: "removeButton"
        enabled: effects.desktops > 1
        icon.name: "list-remove"
        onClicked: effects.desktops--
    }
    Plasma.Button {
        objectName: "addButton"
        enabled: effects.desktops < 20
        icon.name: "list-add"
        onClicked: effects.desktops++
    }
}
