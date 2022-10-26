/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import org.kde.kwin 3.0 as KWinComponents

QtObject {
    id: root

    property QtObject loader: Loader {
        id: guideLoader
    }

    function show() {
        guideLoader.setSource("InteractiveMoveGuide.qml", {
            "handle": KWinComponents.Workspace.interactiveMoveResizeWindow
        });
    }

    function hide() {
        guideLoader.source = "";
    }

    Component.onCompleted: {
        KWinComponents.Workspace.interactiveMoveResizeWindowChanged.connect(() => {
            const window = KWinComponents.Workspace.interactiveMoveResizeWindow;
            if (window && window.move) {
                show();
            } else {
                hide();
            }
        });
    }
}
