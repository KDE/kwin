/*
    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtMultimedia as Multimedia

Multimedia.Video {
    id: videoItem

    source: model.VideoRole
    width: 400
    height: 400

    QQC2.BusyIndicator {
        anchors.centerIn: parent
        visible: videoItem.status === Multimedia.MediaPlayer.Loading
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
    }

    onStopped: {
        replayButton.visible = true
    }
}
