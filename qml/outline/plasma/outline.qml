/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 * Copyright 2017  Kai Uwe Broulik <kde@privat.broulik.de>
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
*/
import QtQuick 2.1
import QtQuick.Window 2.1
import org.kde.plasma.core 2.0 as PlasmaCore

Window {
    id: window

    readonly property int animationDuration: units.longDuration
    property bool animationEnabled: false

    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    color: "transparent"

    // outline is a context property
    x: outline.unifiedGeometry.x
    y: outline.unifiedGeometry.y
    width: outline.unifiedGeometry.width
    height: outline.unifiedGeometry.height

    visible: outline.active

    onVisibleChanged: {
        if (visible) {
            if (outline.visualParentGeometry.width > 0 && outline.visualParentGeometry.height > 0) {
                window.animationEnabled = false
                // move our frame to the visual parent geometry
                svg.setGeometry(outline.visualParentGeometry)
                window.animationEnabled = true
                // and then animate it nicely to its destination
                svg.setGeometry(outline.geometry)
            } else {
                // no visual parent? just move it to its destination right away
                window.animationEnabled = false
                svg.setGeometry(outline.geometry)
                window.animationEnabled = true
            }
        }
    }

    Connections {
        target: outline
        // when unified geometry changes, this means our window position changed and any
        // animation will potentially be offset and/or cut off, skip the animation in this case
        onUnifiedGeometryChanged: {
            if (window.visible) {
                window.animationEnabled = false
                svg.setGeometry(outline.geometry)
                window.animationEnabled = true
            }
        }
    }

    PlasmaCore.FrameSvgItem {
        id: svg

        // takes into account the offset inside unified geometry
        function setGeometry(geometry) {
            x = geometry.x - outline.unifiedGeometry.x
            y = geometry.y - outline.unifiedGeometry.y
            width = geometry.width
            height = geometry.height
        }

        imagePath: "widgets/translucentbackground"

        x: 0
        y: 0
        width: 0
        height: 0

        enabledBorders: {
            var maximizedArea = workspace.clientArea(workspace.MaximizeArea, Qt.point(outline.geometry.x, outline.geometry.y), workspace.currentDesktop);

            var left = outline.geometry.x === maximizedArea.x;
            var right = outline.geometry.x + outline.geometry.width === maximizedArea.x + maximizedArea.width;
            var top = outline.geometry.y === maximizedArea.y;
            var bottom = outline.geometry.y + outline.geometry.height === maximizedArea.y + maximizedArea.height;

            var borders = PlasmaCore.FrameSvgItem.AllBorders;
            if (left) {
                borders = borders & ~PlasmaCore.FrameSvgItem.LeftBorder;
            }
            if (right) {
                borders = borders & ~PlasmaCore.FrameSvgItem.RightBorder;
            }
            if (top) {
                borders = borders & ~PlasmaCore.FrameSvgItem.TopBorder;
            }
            if (bottom) {
                borders = borders & ~PlasmaCore.FrameSvgItem.BottomBorder;
            }
            if (left && right && bottom && top) {
                borders = PlasmaCore.FrameSvgItem.AllBorders;
            }
            return borders;
        }

        Behavior on x {
            NumberAnimation { duration: window.animationDuration; easing.type: Easing.InOutCubic; }
            enabled: window.animationEnabled
        }
        Behavior on y {
            NumberAnimation { duration: window.animationDuration; easing.type: Easing.InOutCubic; }
            enabled: window.animationEnabled
        }
        Behavior on width {
            NumberAnimation { duration: window.animationDuration; easing.type: Easing.InOutCubic; }
            enabled: window.animationEnabled
        }
        Behavior on height {
            NumberAnimation { duration: window.animationDuration; easing.type: Easing.InOutCubic; }
            enabled: window.animationEnabled
        }
    }
}
