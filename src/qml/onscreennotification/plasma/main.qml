/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window

import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami 2.20 as Kirigami

PlasmaCore.Dialog {
    location: PlasmaCore.Types.Floating
    visible: osd.visible
    flags: Qt.FramelessWindowHint
    type: PlasmaCore.Dialog.OnScreenDisplay
    outputOnly: true

    mainItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing
        PlasmaCore.IconItem {
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: implicitWidth
            source: osd.iconName
            visible: osd.iconName !== ""
        }
        QQC2.Label {
            text: osd.message
        }
    }
}
