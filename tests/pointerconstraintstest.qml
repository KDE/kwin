/********************************************************************
Copyright 2018 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 2.10
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.1

ColumnLayout {
/* for margins */
ColumnLayout {
    id: root
    focus: true

    Layout.margins: 20

    function lock() {
        org_kde_kwin_tests_pointerconstraints_backend.lockRequest(lockPersChck.checked, root.activRect());
    }
    function confine() {
        org_kde_kwin_tests_pointerconstraints_backend.confineRequest(confPersChck.checked, root.activRect());
    }
    function unlock() {
        org_kde_kwin_tests_pointerconstraints_backend.unlockRequest();
    }
    function unconfine() {
        org_kde_kwin_tests_pointerconstraints_backend.unconfineRequest();
    }
    function hideAndConfine() {
        org_kde_kwin_tests_pointerconstraints_backend.hideAndConfineRequest();
    }
    function undoHideAndConfine() {
        org_kde_kwin_tests_pointerconstraints_backend.undoHideRequest();
    }

    property bool waylandNative: org_kde_kwin_tests_pointerconstraints_backend.mode === 0

    Keys.onPressed: {
        if (event.key === Qt.Key_L) {
            root.lock();
            event.accepted = true;
        } else if (event.key === Qt.Key_C) {
            root.confine();
            event.accepted = true;
        } else if (event.key === Qt.Key_K) {
            root.unlock();
            event.accepted = true;
        } else if (event.key === Qt.Key_X) {
            root.unconfine();
            event.accepted = true;
        } else if (event.key === Qt.Key_H) {
            root.hideAndConfine();
            event.accepted = true;
        } else if (event.key === Qt.Key_G) {
            root.undoHideAndConfine();
            event.accepted = true;
        }
    }

    function activRect() {
        if (fullWindowChck.checked) {
            return Qt.rect(0, 0, -1, -1);
        }
        return activArea.rect();
    }

    Connections {
        target: org_kde_kwin_tests_pointerconstraints_backend
        onForceSurfaceCommit: {
            forceCommitRect.visible = true
        }
    }

    Rectangle {
        id: forceCommitRect
        width: 10
        height: 10
        color: "red"
        visible: false

        Timer {
            interval: 500
            running: forceCommitRect.visible
            repeat: false
            onTriggered: forceCommitRect.visible = false;
        }
    }

    GridLayout {
        columns: 2
        rowSpacing: 10
        columnSpacing: 10

        Button {
            id: lockButton
            text: "Lock pointer"
            onClicked: root.lock()
        }
        CheckBox {
            id: lockPersChck
            text: "Persistent lock"
            checked: root.waylandNative
            enabled: root.waylandNative
        }
        Button {
            id: confButton
            text: "Confine pointer"
            onClicked: root.confine()
        }
        CheckBox {
            id: confPersChck
            text: "Persistent confine"
            checked: root.waylandNative
            enabled: root.waylandNative
        }
        Button {
            id: hideConfButton
            text: "Hide and confine pointer"
            onClicked: root.hideAndConfine()
            visible: !root.waylandNative
        }
        CheckBox {
            id: confBeforeHideChck
            text: "Confine first, then hide"
            checked: false
            visible: !root.waylandNative
        }
    }

    CheckBox {
        id: lockHintChck
        text: "Send position hint on lock"
        checked: root.waylandNative
        enabled: root.waylandNative
        onCheckedChanged: org_kde_kwin_tests_pointerconstraints_backend.lockHint = checked;
    }
    CheckBox {
        id: restrAreaChck
        text: "Restrict input area (not yet implemented)"
        enabled: false
    }
    CheckBox {
        id: fullWindowChck
        text: "Full window area activates"
        checked: !root.waylandNative
        enabled: root.waylandNative
    }
    CheckBox {
        id: errorsChck
        text: "Allow critical errors"
        checked: false
        enabled: root.waylandNative
        onCheckedChanged: org_kde_kwin_tests_pointerconstraints_backend.errorsAllowed = checked;
    }

    Item {
        width: childrenRect.width
        height: childrenRect.height

        Rectangle {
            id: activArea

            width: 400
            height: 200

            enabled: root.waylandNative && !fullWindowChck.checked

            function rect() {
                var globalPt = mapToGlobal(x, y);
                return Qt.rect(globalPt.x, globalPt.y, width, height);
            }

            border.color: enabled ? "black" : "lightgrey"
            border.width: 2

            Label {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Activation area"
            }
        }
        Button {
            id: unconfButton
            anchors.horizontalCenter: activArea.horizontalCenter
            anchors.verticalCenter: activArea.verticalCenter

            text: "Unconfine pointer"
            onClicked: root.unconfine()
        }
    }

    Label {
        text: "Lock: L / Unlock: K"
    }
    Label {
        text: "Confine: C / Unconfine: X"
    }
    Label {
        text: "Hide cursor and confine pointer: H / undo hide: G"
        visible: !root.waylandNative
    }
}

}
