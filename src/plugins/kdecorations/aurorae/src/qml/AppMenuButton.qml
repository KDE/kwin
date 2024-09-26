/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin.decoration

Item {
    id: appMenuButton
    visible: decoration.client.hasApplicationMenu
    AuroraeButton {
        id: primary
        anchors.fill: parent
        buttonType: DecorationOptions.DecorationButtonApplicationMenu
        visible: auroraeTheme.appMenuButtonPath
    }
    DecorationButton {
        id: fallback
        anchors.fill: parent
        buttonType: DecorationOptions.DecorationButtonApplicationMenu
        visible: !auroraeTheme.appMenuButtonPath
        Kirigami.Icon {
            anchors.fill: parent
            source: decoration.client.icon
        }
    }
}
