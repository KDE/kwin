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
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.0
import org.kde.kwin.kwincompositing 1.0

Rectangle {
    id: item
    width: parent.width
    height: childrenRect.height
    color: item.ListView.isCurrentItem ? effectView.backgroundActiveColor : index % 2 ? effectView.backgroundNormalColor : effectView.backgroundAlternateColor
    signal changed()
    property bool checked: model.EffectStatusRole

    MouseArea {
        anchors.fill: parent
        onClicked: {
            effectView.currentIndex = index;
        }
    }

    RowLayout {
        id: rowEffect
        width: parent.width - 2 * spacing
        x: spacing

        RadioButton {
            id: exclusiveGroupButton
            property bool exclusive: model.ExclusiveRole != ""
            visible: exclusive
            checked: model.EffectStatusRole
            property bool actuallyChanged: true
            property bool initiallyChecked: false
            exclusiveGroup: exclusive ? effectView.exclusiveGroupForCategory(model.ExclusiveRole) : null
            onCheckedChanged: {
                if (!visible) {
                    return;
                }
                actuallyChanged = true;
                item.checked = exclusiveGroupButton.checked
                item.changed();
            }
            onClicked: {
                if (!actuallyChanged || initiallyChecked) {
                    checked = false;
                }
                actuallyChanged = false;
                initiallyChecked = false;
            }
            Component.onCompleted: {
                exclusiveGroupButton.initiallyChecked = model.EffectStatusRole;
            }
        }

        CheckBox {
            function isWindowManagementEnabled() {
                if (model.ServiceNameRole == "kwin4_effect_dialogparent") {
                    windowManagementEnabled = effectStatusCheckBox.checked;
                    return windowManagementEnabled = effectStatusCheckBox.checked && windowManagementEnabled;
                } else if (model.ServiceNameRole == "kwin4_effect_desktopgrid") {
                    windowManagementEnabled = effectStatusCheckBox.checked;
                    return windowManagementEnabled = effectStatusCheckBox.checked && windowManagementEnabled;
                } else if (model.ServiceNameRole == "kwin4_effect_presentwindows") {
                    windowManagementEnabled = effectStatusCheckBox.checked;
                    return windowManagementEnabled = effectStatusCheckBox.checked && windowManagementEnabled;
                }
                return windowManagementEnabled;
            }

            id: effectStatusCheckBox
            property bool windowManagementEnabled;
            checked: model.EffectStatusRole
            visible: model.ExclusiveRole == ""

            onCheckedChanged: {
                if (!visible) {
                    return;
                }
                item.checked = effectStatusCheckBox.checked;
                item.changed();
            }
            Connections {
                target: searchModel
                onDataChanged: {
                    effectStatusCheckBox.checked = model.EffectStatusRole;
                }
            }
        }

        ColumnLayout {
            id: effectItem
            Text {
                text: model.NameRole
                font.bold: true
            }
            Text {
                id: desc
                text: model.DescriptionRole
            }
            Text {
                id:aboutItem
                text: "Author: " + model.AuthorNameRole + "\n" + "License: " + model.LicenseRole
                font.bold: true
                visible: false
            }
            Video {
                id: videoItem
                visible: false
            }
        }
        Item {
            // spacer
            Layout.fillWidth: true
        }

        Button {
            id: videoButton
            visible: model.VideoRole.toString() !== ""
            iconName: "video"
            onClicked: videoItem.showHide()
        }
        Button {
            id: configureButton
            visible: effectConfig.effectUiConfigExists(model.ServiceNameRole)
            enabled: effectStatusCheckBox.checked
            iconName: "configure"
            onClicked: {
                effectConfig.openConfig(model.NameRole);
            }
        }

        Button {
            id: aboutButton
            iconName: "dialog-information"
            onClicked: {
                aboutItem.visible = !aboutItem.visible;
            }
        }
    } //end Row
} //end Rectangle
