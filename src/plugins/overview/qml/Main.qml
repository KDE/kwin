/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
    SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo@venerandi.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.milou as Milou
import org.kde.plasma.components as PC3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kcmutils as KCM

Rectangle {
    readonly property QtObject effect: KWinComponents.SceneView.effect

    focus: true
    color: "white"

    Rectangle {
		color: "red"
        width: 10
        height: 10
		PropertyAnimation on x {
			from: 0
			to: 120
			duration: 1000
			running: true
		}
		onXChanged: console.log(x)
	}

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) {
            effect.deactivate();
        }
    }
}
