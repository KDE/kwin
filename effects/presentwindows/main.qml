/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import org.kde.plasma.components 2.0 as Plasma

Plasma.Button {
    id: closeButton
    iconSource: "window-close"
    anchors.fill: parent
    implicitWidth: units.iconSizes.medium
    implicitHeight: implicitWidth
}
