/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick
import QtQuick.Window
import org.kde.kwin
import org.kde.ksvg 1.0 as KSvg
import org.kde.kirigami 2 as Kirigami

Window {
    id: window

    readonly property int animationDuration: Kirigami.Units.longDuration
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
        function onUnifiedGeometryChanged() {
            if (window.visible) {
                window.animationEnabled = false
                svg.setGeometry(outline.geometry)
                window.animationEnabled = true
            }
        }
    }

    KSvg.FrameSvgItem {
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
            var maximizedArea = Workspace.clientArea(Workspace.MaximizeArea, Workspace.screenAt(Qt.point(outline.geometry.x, outline.geometry.y)), Workspace.currentDesktop);

            var left = outline.geometry.x === maximizedArea.x;
            var right = outline.geometry.x + outline.geometry.width === maximizedArea.x + maximizedArea.width;
            var top = outline.geometry.y === maximizedArea.y;
            var bottom = outline.geometry.y + outline.geometry.height === maximizedArea.y + maximizedArea.height;

            var borders = KSvg.FrameSvgItem.AllBorders;
            if (left) {
                borders = borders & ~KSvg.FrameSvgItem.LeftBorder;
            }
            if (right) {
                borders = borders & ~KSvg.FrameSvgItem.RightBorder;
            }
            if (top) {
                borders = borders & ~KSvg.FrameSvgItem.TopBorder;
            }
            if (bottom) {
                borders = borders & ~KSvg.FrameSvgItem.BottomBorder;
            }
            if (left && right && bottom && top) {
                borders = KSvg.FrameSvgItem.AllBorders;
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
