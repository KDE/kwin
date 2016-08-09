/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
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
import QtQuick 2.1;
import QtQuick.Window 2.1;
import org.kde.plasma.core 2.0 as PlasmaCore;

Window {
    id: window
    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    color: "transparent"

    // outline is a context property
    x: outline.geometry.x
    y: outline.geometry.y
    width: outline.geometry.width
    height: outline.geometry.height
    visible: outline.active

    PlasmaCore.FrameSvgItem {
        function updateBorders() {
            var maximizedArea = workspace.clientArea(workspace.MaximizeArea, Qt.point(outline.geometry.x, outline.geometry.y), workspace.currentDesktop);
            var left = false;
            var right = false;
            var top = false;
            var bottom = false;
            if (outline.geometry.x == maximizedArea.x) {
                left = true;
            }
            if (outline.geometry.y == maximizedArea.y) {
                top = true;
            }
            if (outline.geometry.x + outline.geometry.width == maximizedArea.x + maximizedArea.width) {
                right = true;
            }
            if (outline.geometry.y + outline.geometry.height == maximizedArea.y + maximizedArea.height) {
                bottom = true;
            }
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
            svg.enabledBorders = borders;
        }
        id: svg
        imagePath: "widgets/translucentbackground"
        anchors.fill: parent
        Connections {
            target: outline
            onGeometryChanged: svg.updateBorders()
        }
        Component.onCompleted: svg.updateBorders()
    }
}
