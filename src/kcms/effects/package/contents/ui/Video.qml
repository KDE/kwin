/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.5
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Layouts 1.1
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
    QQC2.Button {
        id: replayButton
        visible: false
        anchors.centerIn: parent
        icon.name: "media-playback-start"
        onClicked: {
            replayButton.visible = false;
            videoItem.play();
        }
        Connections {
            target: videoItem
            function onStopped() {
                replayButton.visible = true
            }
        }
    }
}
