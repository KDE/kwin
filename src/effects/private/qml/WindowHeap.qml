/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Window 2.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
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
    property alias delegate: windowsRepeater.delegate
    readonly property alias count: windowsRepeater.count

    property alias layout: expoLayout
    property int selectedIndex: -1
    property int animationDuration: PlasmaCore.Units.longDuration
    property bool animationEnabled: false
    property bool absolutePositioning: true
    property real padding: 0
    // Either a string "activeClass" or a list internalIds of clients
    property var showOnly: []

    required property bool organized
    readonly property bool effectiveOrganized: expoLayout.ready && organized
    property bool dragActive: false

    signal activated()
    //TODO: for 5.26 the delegate will be a separate component instead
    signal windowClicked(QtObject window, EventPoint eventPoint)

    function activateIndex(index) {
        KWinComponents.Workspace.activeClient = windowsRepeater.itemAt(index).client;
        activated();
    }

    property var dndManagerStore: ({})

    function saveDND(key: int, rect: rect) {
        dndManagerStore[key] = rect;
    }
    function containsDND(key: int): bool {
        return key in dndManagerStore;
    }
    function restoreDND(key: int): rect {
        return dndManagerStore[key];
    }
    function deleteDND(key: int) {
        delete dndManagerStore[key];
    }

    KWinComponents.WindowThumbnailItem {
        id: otherScreenThumbnail
        z: 2
        property KWinComponents.WindowThumbnailItem cloneOf
        visible: false
        client: cloneOf ? cloneOf.client : null
        width: cloneOf ? cloneOf.width : 0
        height: cloneOf ? cloneOf.height : 0
        onCloneOfChanged: {
            if (!cloneOf) {
                visible = false;
            }
        }
    }

    Connections {
        target: effect
        function onItemDraggedOutOfScreen(item, screens) {
            let found = false;

            // don't put a proxy for item's own screen
            if (screens.length === 0 || item.screen === targetScreen) {
                otherScreenThumbnail.visible = false;
                return;
            }

            for (let i in screens) {
                if (targetScreen === screens[i]) {
                    found = true;
                    let globalPos = item.screen.mapToGlobal(item.mapToItem(null, 0,0));
                    let heapRelativePos = targetScreen.mapFromGlobal(globalPos);
                    heapRelativePos = heap.mapFromItem(null, heapRelativePos.x, heapRelativePos.y);
                    otherScreenThumbnail.cloneOf = item
                    otherScreenThumbnail.x = heapRelativePos.x;
                    otherScreenThumbnail.y = heapRelativePos.y;
                    otherScreenThumbnail.visible = true;
                }
            }

            if (!found) {
                otherScreenThumbnail.visible = false;
            }
        }
        function onItemDroppedOutOfScreen(pos, item, screen) {
            if (screen === targetScreen) {
                // To actually move we neeed a screen number rather than an EffectScreen
                KWinComponents.Workspace.sendClientToScreen(item.client, KWinComponents.Workspace.screenAt(pos));
            }
        }
    }

    ExpoLayout {
        id: expoLayout

        anchors.fill: parent
        anchors.margins: heap.padding
        spacing: PlasmaCore.Units.smallSpacing * 5

        Repeater {
            id: windowsRepeater

            onItemAdded: (index, item) => {
                // restore/reparent from drop
                var key = item.client.internalId;
                if (heap.containsDND(key)) {
                    expoLayout.forceLayout();
                    var oldGlobalRect = heap.restoreDND(key);
                    item.restoreDND(oldGlobalRect);
                    heap.deleteDND(key);
                } else if (heap.effectiveOrganized) {
                    // New window has opened in the middle of a running effect.
                    // Make sure it is positioned before enabling its animations.
                    expoLayout.forceLayout();
                }
                item.animationEnabled = true;
            }
            delegate: WindowHeapDelegate {
                windowHeap: heap
            }
        }
    }

    function findFirstItem() {
        for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
            const candidateItem = windowsRepeater.itemAt(candidateIndex);
            if (!candidateItem.hidden) {
                return candidateIndex;
            }
        }
        return -1;
    }

    function findNextItem(selectedIndex, direction) {
        if (selectedIndex === -1) {
            return findFirstItem();
        }

        const selectedItem = windowsRepeater.itemAt(selectedIndex);
        let nextIndex = -1;

        switch (direction) {
        case WindowHeap.Direction.Left:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width < selectedItem.x + selectedItem.width) {
                    if (nextIndex === -1) {
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
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (selectedItem.x < candidateItem.x) {
                    if (nextIndex === -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextIndex === -1 || candidateItem.x < nextItem.x) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Up:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height < selectedItem.y + selectedItem.height) {
                    if (nextIndex === -1) {
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
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (selectedItem.y < candidateItem.y) {
                    if (nextIndex === -1) {
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

    function resetSelected() {
        selectedIndex = -1;
    }

    function selectNextItem(direction) {
        const nextIndex = findNextItem(selectedIndex, direction);
        if (nextIndex !== -1) {
            selectedIndex = nextIndex;
            return true;
        }
        return false;
    }

    function selectLastItem(direction) {
        let last = selectedIndex;
        while (true) {
            const next = findNextItem(last, direction);
            if (next === -1) {
                break;
            } else {
                last = next;
            }
        }
        if (last !== -1) {
            selectedIndex = last;
            return true;
        }
        return false;
    }

    onActiveFocusChanged: resetSelected();

    Keys.onPressed: {
        let handled = false;
        switch (event.key) {
        case Qt.Key_Up:
            handled = selectNextItem(WindowHeap.Direction.Up);
            break;
        case Qt.Key_Down:
            handled = selectNextItem(WindowHeap.Direction.Down);
            break;
        case Qt.Key_Left:
            handled = selectNextItem(WindowHeap.Direction.Left);
            break;
        case Qt.Key_Right:
            handled = selectNextItem(WindowHeap.Direction.Right);
            break;
        case Qt.Key_Home:
            handled = selectLastItem(WindowHeap.Direction.Left);
            break;
        case Qt.Key_End:
            handled = selectLastItem(WindowHeap.Direction.Right);
            break;
        case Qt.Key_PageUp:
            handled = selectLastItem(WindowHeap.Direction.Up);
            break;
        case Qt.Key_PageDown:
            handled = selectLastItem(WindowHeap.Direction.Down);
            break;
        case Qt.Key_Return:
        case Qt.Key_Space:
            handled = true;
            let selectedItem = null;
            if (selectedIndex !== -1) {
                selectedItem = windowsRepeater.itemAt(selectedIndex);
            } else {
                // If the window heap has only one visible window, activate it.
                for (let i = 0; i < windowsRepeater.count; ++i) {
                    const candidateItem = windowsRepeater.itemAt(i);
                    if (candidateItem.hidden) {
                        continue;
                    } else if (selectedItem) {
                        selectedItem = null;
                        break;
                    }
                    selectedItem = candidateItem;
                }
            }
            if (selectedItem) {
                handled = true;
                KWinComponents.Workspace.activeClient = selectedItem.client;
                activated();
            }
            break;
        default:
            return;
        }
        event.accepted = handled;
    }
}
