/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Window 2.15
import org.kde.kwin 3.0 as KWinComponents
import org.kde.plasma.core 2.0 as PlasmaCore

Window {
    id: root

    readonly property int thumbnailWidth: PlasmaCore.Units.gridUnit * 10
    readonly property int thumbnailHeight: PlasmaCore.Units.gridUnit * 10
    readonly property bool _q_showWithoutActivating: true

    color: "transparent"
    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    visible: listModel.count > 0
    width: thumbnailWidth
    height: Math.min(listModel.count * thumbnailHeight, Screen.height)
    x: (Screen.virtualX + Screen.width) - width
    y: Screen.virtualY + 0.5 * (Screen.height - height)

    ListView {
        id: listView
        anchors.fill: parent
        model: listModel
        opacity: 0.5
        delegate: KWinComponents.WindowThumbnailItem {
            client: model.client
            width: thumbnailWidth
            height: thumbnailHeight

            TapHandler {
                onTapped: KWinComponents.Workspace.activeClient = client;
            }
        }
        transform: Rotation {
            axis {
                x: 0
                y: 1
                z: 0
            }
            origin {
                x: width
                y: height / 2
            }
            angle: -30
        }
    }

    ListModel {
        id: listModel
    }

    function toggle() {
        const window = KWinComponents.Workspace.activeClient;
        if (!window) {
            return;
        }

        for (let i = 0; i < listModel.count; ++i) {
            if (listModel.get(i)["client"] == window) {
                listModel.remove(i);
                return;
            }
        }

        listModel.append({"client": window});
    }

    function remove(window) {
        for (let i = 0; i < listModel.count; ++i) {
            const item = listModel.get(i);
            if (item["client"] == window) {
                listModel.remove(i);
            }
        }
    }

    Component.onCompleted: {
        KWin.registerWindow(root);
        KWin.registerShortcut("ToggleCurrentThumbnail", "Toggle Thumbnail for Current Window", "Meta+Ctrl+T", toggle);

        KWinComponents.Workspace.clientRemoved.connect(remove);
    }
}
