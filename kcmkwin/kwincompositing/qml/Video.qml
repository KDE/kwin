/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
* Copyright (C) 2014 Martin Gräßlin       <mgraesslin@kde.org>           *
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
import QtMultimedia 5.0 as Multimedia
import org.kde.kquickcontrolsaddons 2.0 as QtExtra

Multimedia.Video {
    id: videoItem
    function showHide() {
        replayButton.visible = false;
        if (videoItem.visible === true) {
            videoItem.stop();
            videoItem.visible = false;
        } else {
            videoItem.visible = true;
            videoItem.play();
        }
    }
    autoLoad: false
    visible: false
    source: model.VideoRole
    width: 400
    height: 400
    BusyIndicator {
        anchors.centerIn: parent
        visible: videoItem.status == Multimedia.MediaPlayer.Loading
        running: true
    }
    MouseArea {
        // it's a mouse area with icon inside to not have an ugly button background
        id: replayButton
        visible: false
        anchors.fill: parent
        onClicked: {
            replayButton.visible = false;
            videoItem.play();
        }
        QtExtra.QIconItem {
            id: replayIcon
            anchors.centerIn: parent
            width: 16
            height: 16
            icon: "media-playback-start"
        }
        Connections {
            target: videoItem
            onStopped: {
                replayButton.visible = true
            }
        }
    }
}
