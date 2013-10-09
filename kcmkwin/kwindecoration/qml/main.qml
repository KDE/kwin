/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2012 Nuno Pinheiro <nuno@oxygen-icons.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 2.0
import org.kde.kwin.kcmdecoration 0.1

ListView {
    id: listView
    x: 0
    y: 0
    model: decorationModel
    highlight: Rectangle {
        width: listView.width
        height: 150
        color: highlightColor
        opacity: 0.5
    }
    highlightMoveDuration: 250
    boundsBehavior: Flickable.StopAtBounds
    delegate: Item {
        objectName: "decorationItem"
        width: listView.width
        height: 150
        PreviewItem {
            id: preview
            anchors.fill: parent
            visible: type == 0
            Component.onCompleted: {
                if (type == 0) {
                    preview.library = model.library;
                }
            }
        }
        Loader {
            source: type == 1 ? "AuroraePreview.qml" : ""
            anchors.fill: parent
        }
        Loader {
            source: type == 2 ? "DecorationPreview.qml" : ""
            anchors.fill: parent
        }
        MouseArea {
            hoverEnabled: false
            anchors.fill: parent
            onClicked: {
                listView.currentIndex = index;
            }
        }
    }
}
