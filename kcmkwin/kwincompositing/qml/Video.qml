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
import QtQuick.Controls 2.0 as QQC2
import QtQuick.Layouts 1.0
import QtMultimedia 5.0 as Multimedia

Multimedia.Video {
    id: videoItem
    autoPlay: true
    source: model.VideoRole
    width: 400
    height: 400
    QQC2.BusyIndicator {
        anchors.centerIn: parent
        visible: videoItem.status == Multimedia.MediaPlayer.Loading
        running: true
    }
    Button {
        id: replayButton
        visible: false
        anchors.centerIn: parent
        iconName: "media-playback-start"
        onClicked: {
            replayButton.visible = false;
            videoItem.play();
        }
        Connections {
            target: videoItem
            onStopped: {
                replayButton.visible = true
            }
        }
    }
}
