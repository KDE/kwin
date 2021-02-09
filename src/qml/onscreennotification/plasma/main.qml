/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

import QtQuick 2.0;
import QtQuick.Window 2.0;
import org.kde.plasma.core 2.0 as PlasmaCore;
import org.kde.plasma.components 2.0 as Plasma;
import QtQuick.Layouts 1.3;

PlasmaCore.Dialog {
    id: dialog
    location: PlasmaCore.Types.Floating
    visible: osd.visible
    flags: Qt.FramelessWindowHint
    type: PlasmaCore.Dialog.OnScreenDisplay
    outputOnly: true

    mainItem: RowLayout {
        PlasmaCore.IconItem {
            implicitWidth: PlasmaCore.Units.iconSizes["medium"]
            implicitHeight: implicitWidth
            source: osd.iconName
            visible: osd.iconName != ""
        }
        Plasma.Label {
            text: osd.message
        }
    }
}
