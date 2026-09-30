/*
    SPDX-FileCopyrightText: 2012 Dan Vratil <dvratil@redhat.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

import QtQuick
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

Rectangle {
    id: root;

    property string outputName;
    property string number;
    property size resolution;
    property double scale;

    readonly property color badgeColor: {
        const colors = [
            Kirigami.Theme.highlightColor,
            Kirigami.Theme.visitedLinkColor,
            Kirigami.Theme.neutralTextColor,
            Kirigami.Theme.positiveTextColor,
            Kirigami.Theme.negativeTextColor,
            Kirigami.Theme.linkColor,
        ];
        const n = parseInt(root.number);
        return (n > 0) ? colors[(n - 1) % colors.length] : Kirigami.Theme.highlightColor;
    }

    color: Kirigami.Theme.backgroundColor
    border {
        color: Kirigami.Theme.textColor
        width: 2
    }

    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    Row {
        id: contentRow
        spacing: 0

        Item {
            width: textColumn.implicitHeight - root.border.width
            height: textColumn.implicitHeight

            Rectangle {
                id: numberCell
                anchors {
                    fill: parent
                    leftMargin: root.border.width
                    topMargin: root.border.width
                    bottomMargin: root.border.width
                }
                color: root.badgeColor

                PlasmaComponents3.Label {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    font.pixelSize: parent.height
                    fontSizeMode: Text.Fit
                    minimumPixelSize: Kirigami.Theme.defaultFont.pixelSize
                    text: root.number
                    color: Kirigami.Theme.highlightedTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Column {
            id: textColumn
            spacing: Kirigami.Units.smallSpacing
            padding: Kirigami.Units.largeSpacing * 2

            PlasmaComponents3.Label {
                id: displayName
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 2
                text: root.outputName
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            PlasmaComponents3.Label {
                id: modeLabel
                anchors.horizontalCenter: parent.horizontalCenter
                text: resolution.width + "x" + resolution.height +
                      (root.scale !== 1 ? "@" + Math.round(root.scale * 100.0) + "%": "")
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
