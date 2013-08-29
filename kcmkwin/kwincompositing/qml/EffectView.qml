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

    CompositingType {
        id: compositingType
    }

    RowLayout {
        id: row
        width: parent.width
        height: parent.height
        CheckBox {
            id: windowManagement
            text: "Improved Window Management"
            checked: false
            anchors.left: col.right
            anchors.top: parent.top
            anchors.topMargin: col.height/2
            onClicked: searchModel.enableWidnowManagement(windowManagement.checked)
        }

        ComboBox {
            id: openGLType
            model: compositingType
            currentIndex: compositingType.currentOpenGLType()
            anchors.top: windowManagement.bottom
            anchors.left: col.right
            onCurrentIndexChanged: apply.enabled = true
        }

        ColumnLayout {
            id: col
            height: parent.height
            Layout.minimumWidth: parent.width - windowManagement.width

            anchors {
                top: parent.top
                left: parent.left
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
                id: searchModel
                filter: searchField.text
                signal effectState(int rowIndex, bool enabled)

                onEffectState: {
                    searchModel.effectStatus(rowIndex, enabled);
                }
            }

            ScrollView {
                id: scroll
                highlightOnFocus: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                anchors {
                    top: searchField.bottom
                    left: parent.left
                    bottom: apply.top
                }
                ListView {
                    id: effectView
                    Layout.fillWidth: true
                    anchors.fill: parent
                    model: searchModel
                    delegate: Effect{}

                    section.property: "CategoryRole"
                    section.delegate: sectionHeading
                }
            }

            ExclusiveGroup {
                id: desktopSwitching
                //Our ExclusiveGroup must me outside of the
                //ListView, otherwise it will not work
            }

            Button {
                id: apply
                text: "Apply"
                enabled: false
                anchors {
                    bottom: parent.bottom
                }

                onClicked: {
                    searchModel.syncConfig();
                    apply.enabled = false;
                    compositingType.syncConfig(openGLType.currentIndex);
                }
            }

        }//End ColumnLayout
    }//End RowLayout
}//End item
