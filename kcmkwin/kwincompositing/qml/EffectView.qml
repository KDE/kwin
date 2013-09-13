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
import org.kde.plasma.components 2.0 as PlasmaComponents

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
            text: i18n("Improved Window Management")
            checked: false
            anchors.left: col.right
            anchors.top: parent.top
            anchors.topMargin: col.height/4
            onClicked: searchModel.enableWidnowManagement(windowManagement.checked)
        }

        ComboBox {
            id: openGLType
            model: compositingType
            currentIndex: compositingType.currentOpenGLType()
            anchors.top: windowManagement.bottom
            anchors.left: col.right
            onCurrentIndexChanged: {
                apply.enabled = true;
                glScaleFilter.visible = currentIndex != 3;
                xrScaleFilter.visible = currentIndex == 3;
                glColorCorrection.enabled = currentIndex !=3 && glColorCorrection !=4;
            }
        }

        Label {
            id: animationText
            text: i18n("Animation Speed:")
            anchors {
                top: openGLType.bottom
                horizontalCenter: windowManagement.horizontalCenter
                topMargin: 20
            }
        }

        Slider {
            id: animationSpeed
            maximumValue: 6.0
            stepSize: 1.0
            tickmarksEnabled: true
            value: compositing.animationSpeed
            anchors {
                top: animationText.bottom
                left: col.right
            }

            onValueChanged: {
                apply.enabled = true;
            }
        }

        Label {
            id: windowThumbnailText
            text: i18n("Keep Window Thumbnails:")
            anchors {
                top: animationSpeed.bottom
                horizontalCenter: windowManagement.horizontalCenter
                topMargin: 20
            }
        }

        ComboBox {
            id: windowThumbnail
            model: [i18n("Always (Breaks Animations)"), i18n("Only for Shown Windows"), i18n("Never")]
            currentIndex: compositing.windowThumbnail
            Layout.fillWidth: true
            anchors {
                top: windowThumbnailText.bottom
                left: col.right
                right: parent.right
            }
            onCurrentIndexChanged: apply.enabled = true;
        }

        Label {
            id: scaleFilterText
            text: i18n("Scale Method:")
            anchors {
                top: windowThumbnail.bottom
                horizontalCenter: windowManagement.horizontalCenter
                topMargin: 20
            }
        }

        ComboBox {
            id: glScaleFilter
            model: [i18n("Crisp"), i18n("Smooth"), i18n("Accurate")]
            visible: openGLType.currentIndex != 3
            currentIndex: compositing.glScaleFilter
            anchors {
                top: scaleFilterText.bottom
                left: col.right
            }
            onCurrentIndexChanged: apply.enabled = true;
        }

        ComboBox {
            id: xrScaleFilter
            model: [i18n("Crisp"), i18n("Smooth (slower)")]
            visible: openGLType.currentIndex == 3
            currentIndex: compositing.xrScaleFilter ? 1 : 0
            anchors {
                top: scaleFilterText.bottom
                left: glScaleFilter.visible ? glScaleFilter.right : col.right
                right: parent.right
            }
            onCurrentIndexChanged: apply.enabled = true;
        }

         CheckBox {
            id: unredirectFullScreen
            text: i18n("Suspend desktop effects for \nfull screen windows")
            checked: compositing.unredirectFullscreen
            anchors.left: col.right
            anchors.top: xrScaleFilter.bottom
            onClicked: apply.enabled = true
        }

        Label {
            id: glSwapStrategyText
            text: i18n("Tearing Prevention (VSync):")
            anchors {
                top: unredirectFullScreen.bottom
                horizontalCenter: windowManagement.horizontalCenter
                topMargin: 20
            }
        }

        ComboBox {
            id: glSwapStrategy
            model: [i18n("Never"), i18n("Automatic"), i18n("Only when cheap"), i18n("Full screen repaints"), i18n("Re-use screen content")]
            currentIndex: compositing.glSwapStrategy
            anchors {
                top: glSwapStrategyText.bottom
                left: col.right
                right: parent.right
            }
            onCurrentIndexChanged: apply.enabled = true;
        }

        CheckBox {
            id: glColorCorrection
            text: i18n("Enable color correction (experimental)")
            checked: compositing.glColorCorrection
            enabled: openGLType.currentIndex != 3 && openGLType.currentIndex != 4
            anchors {
                top: glSwapStrategy.bottom
                left: col.right
            }
            onClicked: apply.enabled = true
        }

        ColumnLayout {
            id: col
            height: parent.height
            Layout.minimumWidth: parent.width - windowManagement.width

            anchors {
                top: parent.top
                left: parent.left
            }

            Label {
                id: hint
                text: i18n("Hint: To find out or configure how to activate an effect, look at the effect's settings.")
                anchors {
                    top: parent.top
                    left: parent.left
                }
            }

            PlasmaComponents.TextField {
                id: searchField
                clearButtonShown: true
                Layout.fillWidth: true
                height: 20
                anchors {
                    top: hint.bottom
                }
                focus: true
            }

            EffectFilterModel {
                id: searchModel
                filter: searchField.text
                signal effectState(int rowIndex, bool enabled)

                onEffectState: {
                    searchModel.updateEffectStatus(rowIndex, enabled);
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
                    property color backgroundActiveColor: searchModel.backgroundActiveColor
                    property color backgroundNormalColor: searchModel.backgroundNormalColor
                    property color backgroundAlternateColor: searchModel.backgroundAlternateColor
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
                text: i18n("Apply")
                enabled: false
                anchors {
                    bottom: parent.bottom
                }

                onClicked: {
                    searchModel.syncConfig();
                    apply.enabled = false;
                    compositingType.syncConfig(openGLType.currentIndex, animationSpeed.value, windowThumbnail.currentIndex, glScaleFilter.currentIndex, xrScaleFilter.currentIndex == 1,
                    unredirectFullScreen.checked, glSwapStrategy.currentIndex, glColorCorrection.checked);
                }
            }

        }//End ColumnLayout
    }//End RowLayout
}//End item
