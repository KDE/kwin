/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

import QtQuick 2.1
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import org.kde.kwin.kwincompositing 1.0
import org.kde.plasma.core 2.0

Item {
    signal changed

    Component {
        id: sectionHeading
        Rectangle {
            width: parent.width
            height:25
            color: "white"

            Text {
                text: section
                font.bold: true
                font.pointSize: 16
                color: "gray"
            }
        }
    }

    EffectConfig {
        id: effectConfig
        onEffectListChanged: {
            searchModel.load()
        }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent

        Label {
            id: hint
            text: i18n("Hint: To find out or configure how to activate an effect, look at the effect's settings.")
            anchors {
                top: parent.top
                left: parent.left
            }
        }

        RowLayout {
            TextField {
                // TODO: needs clear button, missing in Qt
                id: searchField
                placeholderText: i18n("Search")
                Layout.fillWidth: true
                focus: true
            }

            Button {
                id: ghnsButton
                text: i18n("Get New Effects ...")
                iconName: "get-hot-new-stuff"
                onClicked: effectConfig.openGHNS()
            }
        }

        EffectFilterModel {
            id: searchModel
            objectName: "filterModel"
            filter: searchField.text
            signal effectState(int rowIndex, bool enabled)

            onEffectState: {
                searchModel.updateEffectStatus(rowIndex, enabled);
            }
        }

        ScrollView {
            id: scroll
            frameVisible: true
            highlightOnFocus: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            ListView {
                id: effectView
                property color backgroundActiveColor: searchModel.backgroundActiveColor
                property color backgroundNormalColor: searchModel.backgroundNormalColor
                property color backgroundAlternateColor: searchModel.backgroundAlternateColor
                anchors.fill: parent
                model: searchModel
                delegate: Effect{
                    id: effectDelegate
                    Connections {
                        id: effectStateConnection
                        target: null
                        onChanged: searchModel.effectState(index, checked)
                    }
                    Component.onCompleted: {
                        effectStateConnection.target = effectDelegate
                    }
                }

                section.property: "CategoryRole"
                section.delegate: sectionHeading
            }
        }

        ExclusiveGroup {
            id: desktopSwitching
            //Our ExclusiveGroup must me outside of the
            //ListView, otherwise it will not work
        }

    }//End ColumnLayout
    Connections {
        target: searchModel
        onEffectState: changed()
    }
}//End item
