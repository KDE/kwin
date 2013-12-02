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


Rectangle {
    id: window
    width: 780
    height: 480
    color: engine.backgroundViewColor()
    property bool openGLBrokeState: true
    signal changed

    Item {
        id: openGLError
        visible: false
        anchors.fill: parent
        anchors.leftMargin: parent.width/2 - 100
        Text {
            id: openGLErrorText
            text: i18n("OpenGL compositing (the default) has crashed KWin in the past.\n" +
                  "This was most likely due to a driver bug.\n" +
                  "If you think that you have meanwhile upgraded to a stable driver,\n" +
                  "you can reset this protection but be aware that this might result in an immediate crash!\n" +
                  "Alternatively, you might want to use the XRender backend instead.")
        }

        Button {
            id: openGLButton
            text: i18n("Re-enable OpenGL detection")
            anchors.top: openGLErrorText.bottom
            onClicked: {
                openGLBrokeState = compositing.OpenGLIsBroken();
                view.visible = !openGLBrokeState;
                openGLError.visible = openGLBrokeState;
            }
        }
    }

    EffectView{
        id: view
        anchors.fill: parent
        visible: false
        onChanged: {
            window.changed()
        }
    }

    Compositing {
        id: compositing
        objectName: "compositing"
        animationSpeed: view.animationSpeedValue
        windowThumbnail: view.windowThumbnailIndex
        glScaleFilter: view.glScaleFilterIndex
        xrScaleFilter: view.xrScaleFilterChecked
        unredirectFullscreen: view.unredirectFullScreenChecked
        glSwapStrategy: view.glSwapStrategyIndex
        glColorCorrection: view.glColorCorrectionChecked
    }
    Connections {
        target: compositing
        onChanged: window.changed()
    }

    Component.onCompleted: {
        if (compositing.OpenGLIsUnsafe()) {
            openGLError.visible = true;
        } else {
            openGLError.visible = false;
            view.visible = true;
        }
        compositing.reset();
    }
}
