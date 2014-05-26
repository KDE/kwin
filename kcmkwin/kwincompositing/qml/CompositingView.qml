/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
* Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>                 *
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
    signal changed
    property alias animationSpeedValue: animationSpeed.value
    property alias windowThumbnailIndex: windowThumbnail.currentIndex
    property alias glScaleFilterIndex: glScaleFilter.currentIndex
    property bool xrScaleFilterChecked: xrScaleFilter.currentIndex > 0
    property alias unredirectFullScreenChecked: unredirectFullScreen.checked
    property alias glSwapStrategyIndex: glSwapStrategy.currentIndex
    property alias glColorCorrectionChecked: glColorCorrection.checked
    property alias compositingTypeIndex: backend.type
    property bool compositingEnabledChecked: useCompositing.checked
    property alias openGLPlatformInterfaceIndex: openGLPlatformInterface.currentIndex

    implicitWidth: mainLayout.implicitWidth
    implicitHeight: mainLayout.implicitHeight

    CompositingType {
        id: compositingType
    }

    GridLayout {
        id: mainLayout
        columns: 2
        anchors.fill: parent

        CheckBox {
            id: useCompositing
            checked: compositing.compositingEnabled
            text: i18n("Enable compositor on startup")
            Connections {
                target: compositing
                onCompositingEnabledChanged: {
                    useCompositing.checked = compositing.compositingEnabled
                }
            }
            Layout.columnSpan: 2
        }

        Label {
            id: animationText
            text: i18n("Animation Speed:")
            Layout.alignment: Qt.AlignRight
        }

        Slider {
            id: animationSpeed
            maximumValue: 6.0
            stepSize: 1.0
            tickmarksEnabled: true
            value: compositing.animationSpeed
            Layout.fillWidth: true
        }

        Label {
            text: i18n("Rendering backend:")
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: backend
            property int type: 0
            model: compositingType
            textRole: "NameRole"
            onCurrentIndexChanged: {
                type = compositingType.compositingTypeForIndex(currentIndex);
            }
            Component.onCompleted: {
                type = compositingType.compositingTypeForIndex(currentIndex);
            }
            Connections {
                target: compositing
                onCompositingTypeChanged: {
                    backend.currentIndex = compositingType.indexForCompositingType(compositing.compositingType)
                }
            }
            Layout.fillWidth: true
        }

        Label {
            id: scaleFilterText
            text: i18n("Scale Method:")
            visible: glScaleFilter.visible
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            id: glScaleFilter
            model: [i18n("Crisp"), i18n("Smooth"), i18n("Accurate")]
            visible: backend.type != CompositingType.XRENDER_INDEX
            currentIndex: compositing.glScaleFilter
            Layout.fillWidth: true
        }

        Label {
            text: i18n("Scale Method:")
            visible: xrScaleFilter.visible
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            id: xrScaleFilter
            model: [i18n("Crisp"), i18n("Smooth (slower)")]
            visible: backend.type == CompositingType.XRENDER_INDEX
            currentIndex: compositing.xrScaleFilter ? 1 : 0
            Layout.fillWidth: true
        }

        Label {
            id: glSwapStrategyText
            text: i18n("Tearing Prevention (VSync):")
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            id: glSwapStrategy
            model: [i18n("Never"), i18n("Automatic"), i18n("Only when cheap"), i18n("Full screen repaints"), i18n("Re-use screen content")]
            currentIndex: compositing.glSwapStrategy
            Layout.fillWidth: true
        }

        Label {
            id: windowThumbnailText
            text: i18n("Keep Window Thumbnails:")
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            id: windowThumbnail
            model: [i18n("Always (Breaks Animations)"), i18n("Only for Shown Windows"), i18n("Never")]
            currentIndex: compositing.windowThumbnail
            Layout.fillWidth: true
        }

        Label {
            text: i18n("OpenGL Platform Interface:")
            visible: backend.type != CompositingType.XRENDER_INDEX
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            id: openGLPlatformInterface
            model: compositing.openGLPlatformInterfaceModel
            currentIndex: compositing.openGLPlatformInterface
            textRole: "display"
            visible: backend.type != CompositingType.XRENDER_INDEX
            Layout.fillWidth: true
        }

        Label {
            text: i18n("Expert:")
            Layout.alignment: Qt.AlignRight
        }

        CheckBox {
            id: unredirectFullScreen
            checked: compositing.unredirectFullscreen
            text: i18n("Suspend compositor for full screen windows")
            Connections {
                target: compositing
                onUnredirectFullscreenChanged: {
                    unredirectFullScreen.checked = compositing.unredirectFullscreen
                }
            }
        }

        Label {
            text: i18n("Experimental:")
            Layout.alignment: Qt.AlignRight
        }

        CheckBox {
            id: glColorCorrection
            checked: compositing.glColorCorrection
            enabled: backend.type == CompositingType.OPENGL31_INDEX || backend.type == CompositingType.OPENGL20_INDEX
            text: i18n("Enable color correction")
            Connections {
                target: compositing
                onGlColorCorrectionChanged: {
                    glColorCorrection.checked = compositing.glColorCorrection
                }
            }
        }

        Item {
            // spacer
            Layout.fillHeight: true
        }
    }
    Connections {
        target: backend
        onCurrentIndexChanged: changed()
    }
}//End item
