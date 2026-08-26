/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import org.kde.kwin

Item {
    id: windowLoader

    Repeater {
        model: Workspace.screens

        Item { // Repeater can't load non-Item objects, so wrap our window in an Item instead
            required property var modelData

            OsdWindow {
                screen: modelData
            }
        }
    }
}

