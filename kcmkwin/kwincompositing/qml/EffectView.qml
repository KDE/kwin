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
import QtQuick.Controls 2.0 as QQC2
import QtQuick.Layouts 1.0
import org.kde.kwin.kwincompositing 1.0

Rectangle {
    signal changed
    implicitWidth: col.implicitWidth
    implicitHeight: col.implicitHeight

    Component {
        id: sectionHeading
        Rectangle {
            width: parent.width
            implicitHeight: sectionText.implicitHeight + 2 * col.spacing
            color: searchModel.backgroundNormalColor

            QQC2.Label {
                id: sectionText
                x: col.spacing
                y: col.spacing
                text: section
                font.weight: Font.Bold
                color: searchModel.sectionColor
                anchors.horizontalCenter: parent.horizontalCenter
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

        QQC2.Label {
            id: hint
            text: i18n("Hint: To find out or configure how to activate an effect, look at the effect's settings.")
            anchors {
                top: parent.top
                left: parent.left
            }
        }

        RowLayout {
            QQC2.TextField {
                // TODO: needs clear button, missing in Qt
                id: searchField
                placeholderText: i18n("Search")
                Layout.fillWidth: true
                focus: true
            }

            Button {
                iconName: "configure"
                tooltip: i18n("Configure filter")
                menu: Menu {
                    MenuItem {
                        text: i18n("Exclude Desktop Effects not supported by the Compositor")
                        checkable: true
                        checked: searchModel.filterOutUnsupported
                        onTriggered: {
                            searchModel.filterOutUnsupported = !searchModel.filterOutUnsupported;
                        }
                    }
                    MenuItem {
                        text: i18n("Exclude internal Desktop Effects")
                        checkable: true
                        checked: searchModel.filterOutInternal
                        onTriggered: {
                            searchModel.filterOutInternal = !searchModel.filterOutInternal
                        }
                    }
                }
            }
        }

        EffectFilterModel {
            id: searchModel
            objectName: "filterModel"
            filter: searchField.text
        }

        ScrollView {
            id: scroll
            frameVisible: true
            highlightOnFocus: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                color: effectView.backgroundNormalColor
                anchors.fill: parent
            }
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
                model: searchModel
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
                section.delegate: sectionHeading
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
                iconName: "get-hot-new-stuff"
                onClicked: effectConfig.openGHNS()
            }
        }
    }//End ColumnLayout
    Connections {
        target: searchModel
        onDataChanged: changed()
    }
}//End item
