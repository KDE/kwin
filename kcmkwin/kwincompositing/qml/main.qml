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

Item {
    id: window
    width: 640
    height: 480

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

    ColumnLayout {
        id: col
        height: parent.height
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        TextField {
            id: searchField
            Layout.fillWidth: true
            height: 20
            anchors {
                top: parent.top
            }
            focus: true
        }

        EffectFilterModel {
            id:searchModel
            filter: searchField.text
            effectModel: EffectModel {
                id: effectModel
            }
        }


        ScrollView {
            id: scroll
            highlightOnFocus: true
            Layout.fillWidth: true
            anchors {
                top: searchField.bottom
                left: parent.left
                right: parent.right
                bottom: apply.top
            }
            ListView {
                id: effectView
                Layout.fillWidth: true
                anchors.fill: parent
                model: VisualDataModel{
                    model: searchModel
                    delegate: Effect{}
                }

                section.property: "CategoryRole"
                section.delegate: sectionHeading
            }
        }

        Button {
            id: apply
            text: "Apply"
            enabled: false
            anchors {
                bottom: parent.bottom
            }

            onClicked: {
                effectModel.syncConfig();
                effectModel.reload();
                apply.enabled = false;
            }
        }
    }
}
