/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 1.1;
import org.kde.plasma.core 0.1 as PlasmaCore;
import org.kde.plasma.components 0.1 as Plasma;
import org.kde.qtextracomponents 0.1 as QtExtra;
import org.kde.kwin 0.1;

Item {
    id: root
    property int screenWidth: 0
    property int screenHeight: 0
    // we count desktops starting from 0 to have it better match the layout in the Grid
    property int currentDesktop: 0
    property int previousDesktop: 0
    property int animationDuration: 1000
    property bool showGrid: true

    function loadConfig() {
        root.animationDuration = readConfig("PopupHideDelay", 1000);
        if (readConfig("TextOnly", "false") == "true") {
            root.showGrid = false;
        } else {
            root.showGrid = true;
        }
    }
    Component.onCompleted: {
        var screen = workspace.clientArea(KWin.FullScreenArea, workspace.activeScreen, workspace.currentDesktop);
        root.screenWidth = screen.width;
        root.screenHeight = screen.height;
        loadConfig();
    }
    PlasmaCore.Dialog {
        id: dialog
        visible: false
        windowFlags: Qt.X11BypassWindowManagerHint

        mainItem: Item {
            id: dialogItem
            width: childrenRect.width
            height: childrenRect.height
            Plasma.Label {
                id: textElement
                anchors.top: root.showGrid ? parent.top : undefined
                anchors.horizontalCenter: parent.horizontalCenter
                text: workspace.desktopName(workspace.currentDesktop)
            }
            Grid {
                id: view
                property int itemWidth: root.screenWidth * 0.1
                property int itemHeight: root.screenHeight * 0.1
                anchors.top: textElement.bottom
                visible: root.showGrid
                Repeater {
                    model: workspace.desktops
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
                                NumberAnimation { duration: root.animationDuration/2 }
                            }
                        }
                        Item {
                            id: arrowsContainer
                            anchors.fill: parent
                            QtExtra.QIconItem {
                                anchors.fill: parent
                                icon: "go-up"
                                visible: false
                            }
                            QtExtra.QIconItem {
                                anchors.fill: parent
                                icon: "go-down"
                                visible: {
                                    if (root.currentDesktop <= index) {
                                        // don't show for target desktop
                                        return false;
                                    }
                                    if (index < root.previousDesktop) {
                                        return false;
                                    }
                                    if (root.currentDesktop < root.previousDesktop) {
                                        // we only go down if the new desktop is higher
                                        return false;
                                    }
                                    if (Math.floor(root.currentDesktop/view.columns) == Math.floor(index/view.columns)) {
                                        // don't show icons in same row as target desktop
                                        return false;
                                    }
                                    if (root.previousDesktop % view.columns == index % view.columns) {
                                        // show arrows for icons in same column as the previous desktop
                                        return true;
                                    }
                                    return false;
                                }
                            }
                            QtExtra.QIconItem {
                                anchors.fill: parent
                                icon: "go-up"
                                visible: {
                                    if (root.currentDesktop >= index) {
                                        // don't show for target desktop
                                        return false;
                                    }
                                    if (index > root.previousDesktop) {
                                        return false;
                                    }
                                    if (root.currentDesktop > root.previousDesktop) {
                                        // we only go down if the new desktop is higher
                                        return false;
                                    }
                                    if (Math.floor(root.currentDesktop/view.columns) == Math.floor(index/view.columns)) {
                                        // don't show icons in same row as target desktop
                                        return false;
                                    }
                                    if (root.previousDesktop % view.columns == index % view.columns) {
                                        // show arrows for icons in same column as the previous desktop
                                        return true;
                                    }
                                    return false;
                                }
                            }
                            QtExtra.QIconItem {
                                anchors.fill: parent
                                icon: "go-next"
                                visible: {
                                    if (root.currentDesktop <= index) {
                                        // we don't show for desktops not on the path
                                        return false;
                                    }
                                    if (index < root.previousDesktop) {
                                        // we might have to show this icon in case we go up and to the right
                                        if (Math.floor(root.currentDesktop/view.columns) == Math.floor(index/view.columns)) {
                                            // can only happen in same row
                                            if (index % view.columns >= root.previousDesktop % view.columns) {
                                                // but only for items in the same column or after of the previous desktop
                                                return true;
                                            }
                                        }
                                        return false;
                                    }
                                    if (root.currentDesktop < root.previousDesktop) {
                                        // we only go right if the new desktop is higher
                                        return false;
                                    }
                                    if (Math.floor(root.currentDesktop/view.columns) == Math.floor(index/view.columns)) {
                                        // show icons in same row as target desktop
                                        if (index % view.columns < root.previousDesktop % view.columns) {
                                            // but only for items in the same column or after of the previous desktop
                                            return false;
                                        }
                                        return true;
                                    }
                                    return false;
                                }
                            }
                            QtExtra.QIconItem {
                                anchors.fill: parent
                                icon: "go-previous"
                                visible: {
                                    if (root.currentDesktop >= index) {
                                        // we don't show for desktops not on the path
                                        return false;
                                    }
                                    if (index > root.previousDesktop) {
                                        // we might have to show this icon in case we go down and to the left
                                        if (Math.floor(root.currentDesktop/view.columns) == Math.floor(index/view.columns)) {
                                            // can only happen in same row
                                            if (index % view.columns <= root.previousDesktop % view.columns) {
                                                // but only for items in the same column or before the previous desktop
                                                return true;
                                            }
                                        }
                                        return false;
                                    }
                                    if (root.currentDesktop > root.previousDesktop) {
                                        // we only go left if the new desktop is lower
                                        return false;
                                    }
                                    if (Math.floor(root.currentDesktop/view.columns) == Math.floor(index/view.columns)) {
                                        // show icons in same row as target desktop
                                        if (index % view.columns > root.previousDesktop % view.columns) {
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
                                when: index != root.currentDesktop
                                PropertyChanges {
                                    target: activeElement
                                    opacity: 0.0
                                }
                            },
                            State {
                                name: "SELECTED"
                                when: index == root.currentDesktop
                                PropertyChanges {
                                    target: activeElement
                                    opacity: 1.0
                                }
                            }
                        ]
                        Component.onCompleted: {
                            view.state = (index == root.currentDesktop) ? "SELECTED" : "NORMAL"
                        }
                    }
                }
                Component.onCompleted: {
                    columns = workspace.desktopGridWidth;
                    rows = workspace.desktopGridHeight;
                    if (!root.showGrid) {
                        width = 0;
                        height = 0;
                    }
                }
            }
        }
    }

    Timer {
        id: timer
        repeat: false
        interval: root.animationDuration
        onTriggered: dialog.visible = false
    }

    Connections {
        target: workspace
        onCurrentDesktopChanged: {
            if (root.currentDesktop == workspace.currentDesktop - 1) {
                return;
            }
            root.previousDesktop = root.currentDesktop;
            timer.stop();
            root.currentDesktop = workspace.currentDesktop - 1;
            textElement.text = workspace.desktopName(workspace.currentDesktop);
            // screen geometry might have changed
            var screen = workspace.clientArea(KWin.FullScreenArea, workspace.activeScreen, workspace.currentDesktop);
            root.screenWidth = screen.width;
            root.screenHeight = screen.height;
            if (root.showGrid) {
                // non dependable properties might have changed
                view.columns = workspace.desktopGridWidth;
                view.rows = workspace.desktopGridHeight;
                view.width = workspace.desktopGridWidth * view.itemWidth;
                view.height = workspace.desktopGridHeight * view.itemHeight;
            } else {
                view.width = 0;
                view.height = 0;
                dialogItem.width = textElement.width;
                dialogItem.height = textElement.height;
            }
            // position might have changed
            dialog.x = screen.x + screen.width/2 - dialogItem.width/2;
            dialog.y = screen.y + screen.height/2 - dialogItem.height/2;
            // start the hide timer
            timer.start();
            dialog.visible = true;
        }
    }
    Connections {
        target: options
        onConfigChanged: root.loadConfig()
    }
}
