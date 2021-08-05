/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.overview 1.0
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore

FocusScope {
    id: heap

    enum Direction {
        Left,
        Right,
        Up,
        Down
    }

    property alias model: windowsRepeater.model
    property int selectedIndex: -1
    required property bool organized

    ExpoLayout {
        id: expoLayout
        anchors.fill: parent
        mode: effect.layout
        focus: true
        spacing: PlasmaCore.Units.largeSpacing

        // This assumes that the position of the WindowHeap is static.
        readonly property point scenePosition: mapToItem(null, 0, 0)

        Repeater {
            id: windowsRepeater

            Item {
                id: thumb

                required property QtObject client
                required property int index

                readonly property bool selected: heap.selectedIndex == index

                state: {
                    if (heap.organized) {
                        return "active";
                    }
                    return client.minimized ? "initial-minimized" : "initial";
                }
                z: client.stackingOrder

                KWinComponents.WindowThumbnailItem {
                    id: thumbSource
                    anchors.fill: parent
                    wId: thumb.client.internalId
                }

                ExpoCell {
                    id: cell
                    layout: expoLayout
                    naturalX: thumb.client.x - targetScreen.geometry.x - expoLayout.scenePosition.x
                    naturalY: thumb.client.y - targetScreen.geometry.y - expoLayout.scenePosition.y
                    naturalWidth: thumb.client.width
                    naturalHeight: thumb.client.height
                    persistentKey: thumb.client.internalId
                }

                states: [
                    State {
                        name: "initial"
                        PropertyChanges {
                            target: thumb
                            x: thumb.client.x - targetScreen.geometry.x - expoLayout.scenePosition.x
                            y: thumb.client.y - targetScreen.geometry.y - expoLayout.scenePosition.y
                            width: thumb.client.width
                            height: thumb.client.height
                        }
                    },
                    State {
                        name: "initial-minimized"
                        extend: "initial"
                        PropertyChanges {
                            target: thumb
                            opacity: 0
                        }
                    },
                    State {
                        name: "active"
                        PropertyChanges {
                            target: thumb
                            x: cell.x
                            y: cell.y
                            width: cell.width
                            height: cell.height
                        }
                    }
                ]

                component TweenBehavior : Behavior {
                    NumberAnimation {
                        duration: effect.animationDuration
                        easing.type: Easing.InOutCubic
                    }
                }

                TweenBehavior on x {}
                TweenBehavior on y {}
                TweenBehavior on width {}
                TweenBehavior on height {}
                TweenBehavior on opacity {}

                PlasmaCore.FrameSvgItem {
                    anchors.fill: parent
                    anchors.margins: -PlasmaCore.Units.smallSpacing
                    imagePath: "widgets/viewitem"
                    prefix: "hover"
                    z: -1
                    visible: hoverHandler.hovered || selected
                }

                HoverHandler {
                    id: hoverHandler
                    onHoveredChanged: if (hovered != selected) {
                        heap.selectedIndex = -1;
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        KWinComponents.Workspace.activeClient = thumb.client;
                        effect.deactivate();
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.MiddleButton
                    onTapped: thumb.client.closeWindow()
                }

                PC3.Button {
                    LayoutMirroring.enabled: Qt.application.layoutDirection == Qt.RightToLeft
                    icon.name: "window-close"
                    anchors.right: thumbSource.right
                    anchors.rightMargin: PlasmaCore.Units.largeSpacing
                    anchors.top: thumbSource.top
                    anchors.topMargin: PlasmaCore.Units.largeSpacing
                    implicitWidth: PlasmaCore.Units.iconSizes.medium
                    implicitHeight: implicitWidth
                    visible: hovered || hoverHandler.hovered
                    onClicked: thumb.client.closeWindow();
                }

                Component.onDestruction: {
                    if (selected) {
                        heap.selectedIndex = -1;
                    }
                }
            }
        }

        Keys.onPressed: {
            switch (event.key) {
            case Qt.Key_Escape:
                effect.deactivate();
                break;
            case Qt.Key_Up:
                selectNextItem(WindowHeap.Direction.Up);
                break;
            case Qt.Key_Down:
                selectNextItem(WindowHeap.Direction.Down);
                break;
            case Qt.Key_Left:
                selectNextItem(WindowHeap.Direction.Left);
                break;
            case Qt.Key_Right:
                selectNextItem(WindowHeap.Direction.Right);
                break;
            case Qt.Key_Home:
                selectLastItem(WindowHeap.Direction.Left);
                break;
            case Qt.Key_End:
                selectLastItem(WindowHeap.Direction.Right);
                break;
            case Qt.Key_PageUp:
                selectLastItem(WindowHeap.Direction.Up);
                break;
            case Qt.Key_PageDown:
                selectLastItem(WindowHeap.Direction.Down);
                break;
            case Qt.Key_Return:
            case Qt.Key_Escape:
            case Qt.Key_Space:
                if (heap.selectedIndex != -1) {
                    const selectedItem = windowsRepeater.itemAt(heap.selectedIndex);
                    KWinComponents.Workspace.activeClient = selectedItem.client;
                    effect.deactivate();
                }
                break;
            default:
                return;
            }
            event.accepted = true;
        }

        onActiveFocusChanged: {
            heap.selectedIndex = -1;
        }
    }

    function findNextItem(selectedIndex, direction) {
        if (windowsRepeater.count == 0) {
            return -1;
        } else if (selectedIndex == -1) {
            return 0;
        }

        const selectedItem = windowsRepeater.itemAt(selectedIndex);
        let nextIndex = -1;

        switch (direction) {
        case WindowHeap.Direction.Left:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width < selectedItem.x + selectedItem.width) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (candidateItem.x + candidateItem.width > nextItem.x + nextItem.width) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Right:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (selectedItem.x < candidateItem.x) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextIndex == -1 || candidateItem.x < nextItem.x) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Up:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height < selectedItem.y + selectedItem.height) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextItem.y + nextItem.height < candidateItem.y + candidateItem.height) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Down:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (selectedItem.y < candidateItem.y) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (candidateItem.y < nextItem.y) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        }

        return nextIndex;
    }

    function selectNextItem(direction) {
        const nextIndex = findNextItem(heap.selectedIndex, direction);
        if (nextIndex != -1) {
            heap.selectedIndex = nextIndex;
        }
    }

    function selectLastItem(direction) {
        let last = heap.selectedIndex;
        while (true) {
            const next = findNextItem(last, direction);
            if (next == -1) {
                break;
            } else {
                last = next;
            }
        }
        if (last != -1) {
            heap.selectedIndex = last;
        }
    }
}
