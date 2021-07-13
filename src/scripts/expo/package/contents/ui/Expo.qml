/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Window 2.12

Item {
    function stop() {
        for (let i = 0; i < screensRepeater.count; ++i) {
            screensRepeater.itemAt(i).wrapper1.wrapper2.stop();
        }
    }

    Repeater {
        id: screensRepeater
        model: Qt.application.screens

        // ScreenView needs to be wrapped in a QtObject; otherwise it will be a transient
        // window, which is the opposite what we want.
        Item {
            property var wrapper1: QtObject {
                property var wrapper2: ScreenView {
                    screen: modelData
                }
            }
        }
    }
}
