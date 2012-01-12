/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
import QtQuick 1.1
import org.kde.qtextracomponents 0.1 as QtExtra
import org.kde.kwin.aurorae 0.1
import org.kde.plasma.components 0.1 as PlasmaComponents

Item {
    property alias currentIndex: listView.currentIndex
    ListView {
        id: listView
        anchors.fill: parent
        model: decorationModel
        highlight: PlasmaComponents.Highlight {
            width: listView.width - 10
            height: 140
            hover: true
        }
        delegate: Item {
            width: listView.width
            height: 150
            QtExtra.QPixmapItem {
                pixmap: preview
                anchors.fill: parent
                visible: type == 0
            }
            Item {
                id: aurorae
                visible: type == 1
                anchors.fill: parent
                AuroraeTheme {
                    id: auroraeTheme
                    Component.onCompleted: {
                        auroraeTheme.loadTheme(auroraeThemeName);
                        inactiveAurorae.source = auroraeSource;
                        activeAurorae.source = auroraeSource;
                    }
                }
                AuroraeDecoration {
                    id: inactiveAurorae
                    active: false
                    anchors {
                        fill: parent
                        leftMargin: 40 - auroraeTheme.paddingLeft
                        rightMargin: 10 - auroraeTheme.paddingRight
                        topMargin: 10 - auroraeTheme.paddingTop
                        bottomMargin: 40 - auroraeTheme.paddingBottom
                    }
                }
                AuroraeDecoration {
                    id: activeAurorae
                    active: true
                    anchors {
                        fill: parent
                        leftMargin: 10 - auroraeTheme.paddingLeft
                        rightMargin: 40 - auroraeTheme.paddingRight
                        topMargin: 40 - auroraeTheme.paddingTop
                        bottomMargin: 10 - auroraeTheme.paddingBottom
                    }
                }
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
}
