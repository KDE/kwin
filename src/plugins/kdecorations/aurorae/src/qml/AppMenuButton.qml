/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kwin.decoration 0.1

DecorationButton {
    id: appMenuButton
    buttonType: DecorationOptions.DecorationButtonApplicationMenu
    visible: decoration.client.hasApplicationMenu
    PlasmaCore.IconItem {
        usesPlasmaTheme: false
        source: decoration.client.icon
        anchors.fill: parent
    }
}
