/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: MIT
*/

import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents

Window {
    id: root

    readonly property int thumbnailWidth: Kirigami.Units.gridUnit * 10
    readonly property int thumbnailHeight: Kirigami.Units.gridUnit * 10
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
        delegate: KWinComponents.WindowThumbnail {
            client: model.window
            width: thumbnailWidth
            height: thumbnailHeight

            TapHandler {
                onTapped: KWinComponents.Workspace.activeWindow = client;
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
        const window = KWinComponents.Workspace.activeWindow;
        if (!window) {
            return;
        }

        for (let i = 0; i < listModel.count; ++i) {
            if (listModel.get(i)["window"] == window) {
                listModel.remove(i);
                return;
            }
        }

        listModel.append({"window": window});
    }

    function remove(window) {
        for (let i = 0; i < listModel.count; ++i) {
            const item = listModel.get(i);
            if (item["window"] == window) {
                listModel.remove(i);
            }
        }
    }

    KWinComponents.ShortcutHandler {
        name: "Toggle Current Thumbnail"
        text: "Toggle Current Thumbnail"
        sequence: "Meta+Ctrl+T"
        onActivated: toggle()
    }

    Component.onCompleted: {
        KWinComponents.Workspace.windowRemoved.connect(remove);
    }
}
