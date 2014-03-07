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

    OpenGLErrorView {
        visible: window.openGLBrokeState
        anchors.fill: parent
        onActivated: window.openGLBrokeState = compositing.OpenGLIsBroken();
    }

    CompositingView {
        id: view
        anchors.fill: parent
        visible: !window.openGLBrokeState
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
        compositingType: view.compositingTypeIndex
        compositingEnabled: view.compositingEnabledChecked
    }
    Connections {
        target: compositing
        onChanged: window.changed()
    }

    Component.onCompleted: {
        openGLBrokeState = compositing.OpenGLIsUnsafe()
        compositing.reset();
    }
}
