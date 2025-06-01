/*
    SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Shapes as QQS
import org.kde.kirigami as Kirigami

QQS.Shape {
    id: root
    implicitWidth: 200
    implicitHeight: 200

    property alias startX: shapePath.startX
    property alias startY: shapePath.startY

    function addPoint(p: point) {
        shapePath.pathElements.push(pathLineComponent.createObject(root, { x: p.x, y: p.y }));
    }

    function replaceLatestPoint(p: point) {
        Object.assign(shapePath.pathElements[shapePath.pathElements.length - 1], { x: p.x, y: p.y });
    }

    function clearPoints() {
        shapePath.pathElements.forEach((elem) => elem.destroy());
        shapePath.pathElements.splice(0, shapePath.pathElements.length);
    }

    Component {
        id: pathLineComponent
        PathLine {
            required x
            required y
        }
    }

    QQS.ShapePath {
        id: shapePath
        strokeWidth: 8
        strokeColor: Kirigami.Theme.focusColor
        strokeStyle: QQS.ShapePath.SolidLine
        joinStyle: QQS.ShapePath.RoundJoin
        fillColor: "transparent"
        pathHints: QQS.ShapePath.PathLinear | QQS.ShapePath.PathSolid
    }
}
