/*
    SPDX-FileCopyrightText: 2012 Dan Vratil <dvratil@redhat.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

import QtQuick
import org.kde.plasma.components 3.0 as PlasmaComponents3
import org.kde.kirigami 2 as Kirigami

Rectangle {
    id: root;

    property string outputName;
    property size resolution;
    property double scale;

    color: Kirigami.Theme.backgroundColor

    implicitWidth: childrenRect.width + 2 * childrenRect.x
    implicitHeight: childrenRect.height + 2 * childrenRect.y

    PlasmaComponents3.Label {
        id: displayName
        x: Kirigami.Units.largeSpacing * 2
        y: Kirigami.Units.largeSpacing
        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 3
        text: root.outputName;
        wrapMode: Text.WordWrap;
        horizontalAlignment: Text.AlignHCenter;
    }

    PlasmaComponents3.Label {
        id: modeLabel;
        anchors {
            horizontalCenter: displayName.horizontalCenter
            top: displayName.bottom
        }
        text: resolution.width + "x" + resolution.height +
              (root.scale !== 1 ? "@" + Math.round(root.scale * 100.0) + "%": "")
        horizontalAlignment: Text.AlignHCenter;
    }
}
