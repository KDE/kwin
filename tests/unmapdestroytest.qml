/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
import QtQuick 2.1
import QtQuick.Window 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1

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
