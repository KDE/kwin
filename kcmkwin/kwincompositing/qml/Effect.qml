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
    height: rowEffect.implicitHeight
    color: item.ListView.isCurrentItem ? effectView.backgroundActiveColor : index % 2 ? effectView.backgroundNormalColor : effectView.backgroundAlternateColor
    signal changed()
    property int checkedState: model.EffectStatusRole

    MouseArea {
        anchors.fill: parent
        onClicked: {
            effectView.currentIndex = index;
        }
    }

    RowLayout {
        id: rowEffect
        property int maximumWidth: parent.width - 2 * spacing
        width: maximumWidth
        Layout.maximumWidth: maximumWidth
        x: spacing

        RowLayout {
            id: checkBoxLayout
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
                    item.checkedState = exclusiveGroupButton.checked ? Qt.Checked : Qt.Unchecked
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
                id: effectStatusCheckBox
                checkedState: model.EffectStatusRole
                visible: model.ExclusiveRole == ""

                onCheckedStateChanged: {
                    if (!visible) {
                        return;
                    }
                    item.checkedState = effectStatusCheckBox.checkedState;
                    item.changed();
                }
                Connections {
                    target: searchModel
                    onDataChanged: {
                        effectStatusCheckBox.checkedState = model.EffectStatusRole;
                    }
                }
            }
        }

        ColumnLayout {
            id: effectItem
            property int maximumWidth: parent.maximumWidth - checkBoxLayout.width - (videoButton.width + configureButton.width + aboutButton.width) - parent.spacing * 5
            Layout.maximumWidth: maximumWidth
            Label {
                text: model.NameRole
                font.bold: true
                wrapMode: Text.Wrap
                Layout.maximumWidth: parent.maximumWidth
            }
            Label {
                id: desc
                text: model.DescriptionRole
                wrapMode: Text.Wrap
                Layout.maximumWidth: parent.maximumWidth
            }
            Label {
                id:aboutItem
                text: i18n("Author: %1\nLicense: %2", model.AuthorNameRole, model.LicenseRole)
                font.bold: true
                visible: false
                wrapMode: Text.Wrap
                Layout.maximumWidth: parent.maximumWidth
            }
            Loader {
                id: videoItem
                active: false
                source: "Video.qml"
                function showHide() {
                    if (!videoItem.active) {
                        videoItem.active = true;
                    } else {
                        videoItem.item.showHide();
                    }
                }
                onLoaded: {
                    videoItem.item.showHide();
                }
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
            visible: ConfigurableRole
            enabled: item.checkedState != Qt.Unchecked
            iconName: "configure"
            onClicked: {
                effectConfig.openConfig(model.ServiceNameRole, model.ScriptedRole, model.NameRole);
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
