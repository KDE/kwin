/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.1
import QtQuick.Window 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.15

/*
 * Small test application to test the difference between unmap and destroy.
 * The window has two buttons: one called "Unmap" which will hide() the QWindow,
 * one called "Destroy" which will close() the QWindow.
 *
 * The test application can be run with qmlscene:
 *
 * @code
 * qmlscene unmapdestroytest.qml
 * @endcode
 *
 * In order to test different modes, the test application understands some arguments:
 * --unmanaged    Creates an override redirect window
 * --frameless    Creates a frameless window (comparable Plasma Dialog)
 */

Window {
    id: window
    visible: false
    x: 0
    y: 0
    width: layout.implicitWidth
    height: layout.implicitHeight
    color: "black"

    RowLayout {
        id: layout
        Button {
            Timer {
                id: timer
                interval: 2000
                running: false
                repeat: false
                onTriggered: window.show()
            }
            text: "unmap"
            onClicked: {
                timer.start();
                window.hide();
            }
        }
        Button {
            text: "destroy"
            onClicked: window.close()
        }
    }

    Component.onCompleted: {
        var flags = Qt.Window;
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            var argument = Qt.application.arguments[i];
            if (argument == "--unmanaged") {
                flags = flags | Qt.BypassWindowManagerHint;
            }
            if (argument == "--frameless") {
                flags = flags | Qt.FramelessWindowHint
            }
        }
        window.flags = flags;
        window.visible = true;
    }
}
