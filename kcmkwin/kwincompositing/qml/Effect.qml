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

Component {
    id: effectDelegate
    Item {
        id: item
        width: parent.width
        height: 40

        Rectangle {
            id: background
            color: item.ListView.isCurrentItem ? "#448" : index % 2 ? "#eee" : "#fff"
            anchors.fill : parent

            Row {
                CheckBox {
                    function isDesktopSwitching() {
                        if (model.ServiceNameRole == "kwin4_effect_slide") {
                            return true;
                        } else if (model.ServiceNameRole == "kwin4_effect_fadedesktop") {
                            return true;
                        } else if (model.ServiceNameRole == "kwin4_effect_cubeslide") {
                            return true;
                        } else {
                            return false;
                        }
                    }
                    function isWindowManagementEnabled() {
                        if (model.ServiceNameRole == "kwin4_effect_dialogparent") {
                            windowManagementEnabled = myCheckBox.checked;
                            return windowManagementEnabled = myCheckBox.checked && windowManagementEnabled;
                        } else if (model.ServiceNameRole == "kwin4_effect_desktopgrid") {
                            windowManagementEnabled = myCheckBox.checked;
                            return windowManagementEnabled = myCheckBox.checked && windowManagementEnabled;
                        } else if (model.ServiceNameRole == "kwin4_effect_presentwindows") {
                            windowManagementEnabled = myCheckBox.checked;
                            return windowManagementEnabled = myCheckBox.checked && windowManagementEnabled;
                        }
                        return windowManagementEnabled;
                    }

                    id: myCheckBox
                    property bool windowManagementEnabled;
                    checked: model.EffectStatusRole
                    exclusiveGroup: isDesktopSwitching() ? desktopSwitching : null
                    onClicked: {
                        apply.enabled = true;
                    }

                    onCheckedChanged: {
                        windowManagement.checked = isWindowManagementEnabled();
                        searchModel.effectState(index, checked);
                    }
                }

                Item {
                    id: effectItem
                    width: effectView.width - myCheckBox.width - aboutButton.width - configureButton.width
                    anchors.left: myCheckBox.right
                    Column {
                        id: col
                        Text {
                            text: model.NameRole
                        }
                        Text {
                            id: desc
                            text: model.DescriptionRole
                            width: effectItem.width
                            elide: Text.ElideRight
                        }
                        Item {
                            id:aboutItem
                            anchors.top: desc.bottom
                            anchors.topMargin: 20
                            visible: false

                            Text {
                                text: "Author: " + model.AuthorNameRole + "\n" + "License: " + model.LicenseRole
                                font.bold: true
                            }
                            PropertyAnimation {id: animationAbout; target: aboutItem; property: "visible"; to: !aboutItem.visible}
                            PropertyAnimation {id: animationAboutSpacing; target: item; property: "height"; to: item.height == 40 ? 80 : 40}
                        }
                    }
                    MouseArea {
                        id: area
                        width: effectView.width - myCheckBox.width
                        height: effectView.height
                        onClicked: {
                            effectView.currentIndex = index;
                        }
                    }
                }

                Button {
                    id: configureButton
                    anchors.left: effectItem.right
                    visible: effectConfig.effectUiConfigExists(model.ServiceNameRole)
                    width: 50
                    height: 25
                    enabled: myCheckBox.checked
                    onClicked: {
                        effectConfig.openConfig(model.NameRole);
                    }

                    Component.onCompleted: {
                        searchModel.image('actions/configure.png')
                        iconSource = searchModel.imagePath;
                    }

                }

                Button {
                    id: aboutButton
                    anchors.left: configureButton.right
                    width: 50
                    height: 25

                    onClicked: {
                        animationAbout.running = true;
                        animationAboutSpacing.running = true;
                    }

                    Component.onCompleted: {
                        searchModel.image('status/dialog-information.png')
                        iconSource = searchModel.imagePath;
                    }
                }

                EffectConfig {
                    id: effectConfig
                }

            } //end Row
        } //end Rectangle
    } //end item
}

