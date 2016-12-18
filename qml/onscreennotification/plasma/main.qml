/*
 * Copyright 2016  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
    flags: Qt.X11BypassWindowManagerHint | Qt.FramelessWindowHint
    type: PlasmaCore.Dialog.OnScreenDisplay
    outputOnly: true

    mainItem: RowLayout {
        PlasmaCore.IconItem {
            Layout.minimumWidth: 64
            Layout.minimumHeight: 64
            source: osd.iconName
            visible: osd.iconName != ""
        }
        Plasma.Label {
            text: osd.message
        }
    }
}
