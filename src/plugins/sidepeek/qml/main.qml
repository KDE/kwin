/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.extras as PlasmaExtras

Item {
    EffectTogglableState {
        EffectTogglableGesture {
            Component.onCompleted: {
                addTouchpadSwipeGesture(SwipeDirection.Left, 4)
            }
        }
    }
}
