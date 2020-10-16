/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var quitEffect = {
    closed: function (window) {
        if (!window.desktopWindow || effects.sessionState != Globals.Quitting) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        window.outAnimation = animate({
            window: window,
            duration: 30 * 1000, // 30 seconds should be long enough for any shutdown
            type: Effect.Generic, // do nothing, just hold a reference
            from: 0.0,
            to: 0.0
        });
    },
    init: function () {
        effects.windowClosed.connect(quitEffect.closed);
    }
};
quitEffect.init();
