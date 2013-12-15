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
    signal changed
    property alias animationSpeedValue: animationSpeed.value
    property alias windowThumbnailIndex: windowThumbnail.currentIndex
    property alias glScaleFilterIndex: glScaleFilter.currentIndex
    property bool xrScaleFilterChecked: xrScaleFilter.currentIndex > 0
    property alias unredirectFullScreenChecked: unredirectFullScreen.checked
    property alias glSwapStrategyIndex: glSwapStrategy.currentIndex
    property alias glColorCorrectionChecked: glColorCorrection.checked
    property alias compositingTypeIndex: openGLType.type
    property bool compositingEnabledChecked: useCompositing.checked

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
            id: useCompositing
            text: i18n("Enable desktop effects on startup")
            checked: compositing.compositingEnabled
            anchors {
                top: parent.top
                left: col.right
                topMargin: col.height/8
            }
            Connections {
                target: compositing
                onCompositingEnabledChanged: {
                    useCompositing.checked = compositing.compositingEnabled
                }
            }
        }

        CheckBox {
            id: windowManagement
            text: i18n("Improved Window Management")
            checked: false
            anchors.left: col.right
            anchors.top: useCompositing.bottom
            //anchors.topMargin: col.height/8
            onClicked: searchModel.enableWidnowManagement(windowManagement.checked)
        }

        ComboBox {
            id: openGLType
            property int type: 0
            model: compositingType
            textRole: "NameRole"
            anchors.top: windowManagement.bottom
            anchors.left: col.right
            onCurrentIndexChanged: {
                glScaleFilter.visible = currentIndex != 3;
                xrScaleFilter.visible = currentIndex == 3;
                glColorCorrection.enabled = currentIndex !=3 && glColorCorrection !=4;
                type = compositingType.compositingTypeForIndex(currentIndex);
            }
            Component.onCompleted: {
                type = compositingType.compositingTypeForIndex(currentIndex);
            }
            Connections {
                target: compositing
                onCompositingTypeChanged: {
                    openGLType.currentIndex = compositingType.indexForCompositingType(compositing.compositingType)
                }
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
        }

         CheckBox {
            id: unredirectFullScreen
            text: i18n("Suspend desktop effects for \nfull screen windows")
            checked: compositing.unredirectFullscreen
            anchors.left: col.right
            anchors.top: xrScaleFilter.bottom
            Connections {
                target: compositing
                onUnredirectFullscreenChanged: {
                    unredirectFullScreen.checked = compositing.unredirectFullscreen
                }
            }
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
        }

        CheckBox {
            id: glColorCorrection
            text: i18n("Enable color correction \n(experimental)")
            checked: compositing.glColorCorrection
            enabled: openGLType.currentIndex != 3 && openGLType.currentIndex != 4
            anchors {
                top: glSwapStrategy.bottom
                left: col.right
            }
            Connections {
                target: compositing
                onGlColorCorrectionChanged: {
                    glColorCorrection.checked = compositing.glColorCorrection
                }
            }
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
                objectName: "filterModel"
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
                    bottom: parent.bottom
                }
                ListView {
                    id: effectView
                    property color backgroundActiveColor: searchModel.backgroundActiveColor
                    property color backgroundNormalColor: searchModel.backgroundNormalColor
                    property color backgroundAlternateColor: searchModel.backgroundAlternateColor
                    Layout.fillWidth: true
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
    }//End RowLayout
    Connections {
        target: searchModel
        onEffectState: changed()
    }
    Connections {
        target: openGLType
        onCurrentIndexChanged: changed()
    }
}//End item
