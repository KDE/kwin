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
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import org.kde.kwin.kwincompositing 1.0

Rectangle {
    signal changed
    implicitHeight: 500
    implicitWidth: 400

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
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            text: i18n("Hint: To find out or configure how to activate an effect, look at the effect's settings.")
            elide: Text.ElideRight
        }

        RowLayout {
            TextField {
                // TODO: needs clear button, missing in Qt
                id: searchField
                placeholderText: i18n("Search...")
                Layout.fillWidth: true
                focus: true
            }

            Button {
                id: configureButton
                icon.name: "configure"
                ToolTip.visible: hovered
                ToolTip.text: i18n("Configure filter")
                onClicked: menu.opened ?  menu.close() : menu.open()
            }
            Menu {
                id: menu
                y: configureButton.height
                x: parent.width - width
                MenuItem {
                    text: i18n("Exclude Desktop Effects not supported by the Compositor")
                    checkable: true
                    checked: searchModel.filterOutUnsupported
                    onToggled: {
                        searchModel.filterOutUnsupported = !searchModel.filterOutUnsupported;
                    }
                }
                MenuItem {
                    text: i18n("Exclude internal Desktop Effects")
                    checkable: true
                    checked: searchModel.filterOutInternal
                    onToggled: {
                        searchModel.filterOutInternal = !searchModel.filterOutInternal
                    }
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Draw a frame around the scrollview
            Component.onCompleted: background.visible = true;

            ListView {
                function exclusiveGroupForCategory(category) {
                    for (var i = 0; i < effectView.exclusiveGroups.length; ++i) {
                        var item = effectView.exclusiveGroups[i];
                        if (item.category == category) {
                            return item.group;
                        }
                    }
                    var newGroup = Qt.createQmlObject('import QtQuick 2.1; import QtQuick.Controls 1.1; ExclusiveGroup {}',
                                                      effectView,
                                                      "dynamicExclusiveGroup" + effectView.exclusiveGroups.length);
                    effectView.exclusiveGroups[effectView.exclusiveGroups.length] = {
                        'category': category,
                        'group': newGroup
                    };
                    return newGroup;
                }
                id: effectView
                property var exclusiveGroups: []
                property color backgroundNormalColor: searchModel.backgroundNormalColor
                property color backgroundAlternateColor: searchModel.backgroundAlternateColor
                anchors.fill: parent
                model: EffectFilterModel {
                    id: searchModel
                    objectName: "filterModel"
                    filter: searchField.text
                }
                delegate: Effect{
                    id: effectDelegate
                    Connections {
                        id: effectStateConnection
                        target: null
                        onChanged: {
                            searchModel.updateEffectStatus(index, checkedState);
                        }
                    }
                    Component.onCompleted: {
                        effectStateConnection.target = effectDelegate
                    }
                }

                section.property: "CategoryRole"
                section.delegate: Rectangle {
                    width: parent.width
                    implicitHeight: sectionText.implicitHeight + 2 * col.spacing
                    color: searchModel.backgroundNormalColor

                    Label {
                        id: sectionText
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        text: section
                        font.weight: Font.Bold
                        color: searchModel.sectionColor
                    }
                }
                spacing: col.spacing
                focus: true
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }
            Button {
                id: ghnsButton
                text: i18n("Get New Desktop Effects...")
                icon.name: "get-hot-new-stuff"
                onClicked: effectConfig.openGHNS()
            }
        }
    }//End ColumnLayout
    Connections {
        target: searchModel
        onDataChanged: changed()
    }
}//End Rectangle
