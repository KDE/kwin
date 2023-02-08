/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012, 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0;
import QtQuick.Window 2.0;
import org.kde.plasma.core 2.0 as PlasmaCore;
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.kwin 3.0

PlasmaCore.Dialog {
    id: dialog
    location: PlasmaCore.Types.Floating
    visible: false
    flags: Qt.X11BypassWindowManagerHint | Qt.FramelessWindowHint
    outputOnly: true

    mainItem: Item {
        function loadConfig() {
            dialogItem.animationDuration = KWin.readConfig("PopupHideDelay", 1000);
            if (KWin.readConfig("TextOnly", "false") == "true") {
                dialogItem.showGrid = false;
            } else {
                dialogItem.showGrid = true;
            }
        }

        function show() {
            const index = Workspace.desktops.indexOf(Workspace.currentDesktop);
            if (dialogItem.currentIndex === index) {
                return;
            }
            dialogItem.previousIndex = dialogItem.currentIndex;
            timer.stop();
            dialogItem.currentIndex = index;
            // screen geometry might have changed
            var screen = Workspace.clientArea(KWin.FullScreenArea, Workspace.activeScreen, Workspace.currentDesktop);
            dialogItem.screenWidth = screen.width;
            dialogItem.screenHeight = screen.height;
            if (dialogItem.showGrid) {
                // non dependable properties might have changed
                view.columns = Workspace.desktopGridWidth;
                view.rows = Workspace.desktopGridHeight;
            }
            dialog.visible = true;
            // position might have changed
            dialog.x = screen.x + screen.width/2 - dialogItem.width/2;
            dialog.y = screen.y + screen.height/2 - dialogItem.height/2;
            // start the hide timer
            timer.start();
        }

        id: dialogItem
        property int screenWidth: 0
        property int screenHeight: 0
        property int currentIndex: 0
        property int previousIndex: 0
        property int animationDuration: 1000
        property bool showGrid: true

        width: dialogItem.showGrid ? view.itemWidth * view.columns : Math.ceil(textElement.implicitWidth)
        height: dialogItem.showGrid ? view.itemHeight * view.rows + textElement.height : textElement.height

        PlasmaExtras.Heading {
            id: textElement
            anchors.top: dialogItem.showGrid ? parent.top : undefined
            anchors.left: parent.left
            anchors.right: parent.right
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            text: Workspace.currentDesktop.name
        }

        Grid {
            id: view
            columns: 1
            rows: 1
            property int itemWidth: dialogItem.screenWidth * Math.min(0.8/columns, 0.1)
            property int itemHeight: Math.min(itemWidth * (dialogItem.screenHeight / dialogItem.screenWidth), dialogItem.screenHeight * Math.min(0.8/rows, 0.1))
            anchors {
                top: textElement.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            visible: dialogItem.showGrid
            Repeater {
                id: repeater
                model: Workspace.desktops
                Item {
                    width: view.itemWidth
                    height: view.itemHeight
                    PlasmaCore.FrameSvgItem {
                        anchors.fill: parent
                        imagePath: "widgets/pager"
                        prefix: "normal"
                    }
                    PlasmaCore.FrameSvgItem {
                        id: activeElement
                        anchors.fill: parent
                        imagePath: "widgets/pager"
                        prefix: "active"
                        opacity: 0.0
                        Behavior on opacity {
                            NumberAnimation { duration: dialogItem.animationDuration/2 }
                        }
                    }
                    Item {
                        id: arrowsContainer
                        anchors.fill: parent
                        PlasmaCore.IconItem {
                            anchors.fill: parent
                            source: "go-up"
                            visible: false
                        }
                        PlasmaCore.IconItem {
                            anchors.fill: parent
                            source: "go-down"
                            visible: {
                                if (dialogItem.currentIndex <= index) {
                                    // don't show for target desktop
                                    return false;
                                }
                                if (index < dialogItem.previousIndex) {
                                    return false;
                                }
                                if (dialogItem.currentIndex < dialogItem.previousIndex) {
                                    // we only go down if the new desktop is higher
                                    return false;
                                }
                                if (Math.floor(dialogItem.currentIndex/view.columns) == Math.floor(index/view.columns)) {
                                    // don't show icons in same row as target desktop
                                    return false;
                                }
                                if (dialogItem.previousIndex % view.columns == index % view.columns) {
                                    // show arrows for icons in same column as the previous desktop
                                    return true;
                                }
                                return false;
                            }
                        }
                        PlasmaCore.IconItem {
                            anchors.fill: parent
                            source: "go-up"
                            visible: {
                                if (dialogItem.currentIndex >= index) {
                                    // don't show for target desktop
                                    return false;
                                }
                                if (index > dialogItem.previousIndex) {
                                    return false;
                                }
                                if (dialogItem.currentIndex > dialogItem.previousIndex) {
                                    // we only go down if the new desktop is higher
                                    return false;
                                }
                                if (Math.floor(dialogItem.currentIndex/view.columns) == Math.floor(index/view.columns)) {
                                    // don't show icons in same row as target desktop
                                    return false;
                                }
                                if (dialogItem.previousIndex % view.columns == index % view.columns) {
                                    // show arrows for icons in same column as the previous desktop
                                    return true;
                                }
                                return false;
                            }
                        }
                        PlasmaCore.IconItem {
                            anchors.fill: parent
                            source: "go-next"
                            visible: {
                                if (dialogItem.currentIndex <= index) {
                                    // we don't show for desktops not on the path
                                    return false;
                                }
                                if (index < dialogItem.previousIndex) {
                                    // we might have to show this icon in case we go up and to the right
                                    if (Math.floor(dialogItem.currentIndex/view.columns) == Math.floor(index/view.columns)) {
                                        // can only happen in same row
                                        if (index % view.columns >= dialogItem.previousIndex % view.columns) {
                                            // but only for items in the same column or after of the previous desktop
                                            return true;
                                        }
                                    }
                                    return false;
                                }
                                if (dialogItem.currentIndex < dialogItem.previousIndex) {
                                    // we only go right if the new desktop is higher
                                    return false;
                                }
                                if (Math.floor(dialogItem.currentIndex/view.columns) == Math.floor(index/view.columns)) {
                                    // show icons in same row as target desktop
                                    if (index % view.columns < dialogItem.previousIndex % view.columns) {
                                        // but only for items in the same column or after of the previous desktop
                                        return false;
                                    }
                                    return true;
                                }
                                return false;
                            }
                        }
                        PlasmaCore.IconItem {
                            anchors.fill: parent
                            source: "go-previous"
                            visible: {
                                if (dialogItem.currentIndex >= index) {
                                    // we don't show for desktops not on the path
                                    return false;
                                }
                                if (index > dialogItem.previousIndex) {
                                    // we might have to show this icon in case we go down and to the left
                                    if (Math.floor(dialogItem.currentIndex/view.columns) == Math.floor(index/view.columns)) {
                                        // can only happen in same row
                                        if (index % view.columns <= dialogItem.previousIndex % view.columns) {
                                            // but only for items in the same column or before the previous desktop
                                            return true;
                                        }
                                    }
                                    return false;
                                }
                                if (dialogItem.currentIndex > dialogItem.previousIndex) {
                                    // we only go left if the new desktop is lower
                                    return false;
                                }
                                if (Math.floor(dialogItem.currentIndex/view.columns) == Math.floor(index/view.columns)) {
                                    // show icons in same row as target desktop
                                    if (index % view.columns > dialogItem.previousIndex % view.columns) {
                                        // but only for items in the same column or before of the previous desktop
                                        return false;
                                    }
                                    return true;
                                }
                                return false;
                            }
                        }
                    }
                    states: [
                        State {
                            name: "NORMAL"
                            when: index != dialogItem.currentIndex
                            PropertyChanges {
                                target: activeElement
                                opacity: 0.0
                            }
                        },
                        State {
                            name: "SELECTED"
                            when: index == dialogItem.currentIndex
                            PropertyChanges {
                                target: activeElement
                                opacity: 1.0
                            }
                        }
                    ]
                    Component.onCompleted: {
                        view.state = (index == dialogItem.currentIndex) ? "SELECTED" : "NORMAL"
                    }
                }
            }
        }

        Timer {
            id: timer
            repeat: false
            interval: dialogItem.animationDuration
            onTriggered: dialog.visible = false
        }

        Connections {
            target: Workspace
            function onCurrentDesktopChanged() {
                dialogItem.show()
            }
            function onNumberDesktopsChanged() {
                repeater.model = Workspace.desktops;
            }
        }
        Connections {
            target: Options
            function onConfigChanged() {
                dialogItem.loadConfig()
            }
        }
        Component.onCompleted: {
            view.columns = Workspace.desktopGridWidth;
            view.rows = Workspace.desktopGridHeight;
            dialogItem.loadConfig();
            dialogItem.show();
        }
    }

    Component.onCompleted: {
        KWin.registerWindow(dialog);
    }
}
