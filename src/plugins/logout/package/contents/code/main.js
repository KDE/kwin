/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var logoutEffect = {
    isLogoutWindow: function (window) {
        if (window.windowClass === "ksmserver-logout-greeter ksmserver-logout-greeter") {
            return true;
        }
        return false;
    },
    opened: function (window) {
        if (!logoutEffect.isLogoutWindow(window)) {
            return;
        }
        window.inAnimation = animate({
            window: window,
            duration: animationTime(400),
            type: Effect.Opacity,
            from: 0.0,
            to: 1.0
        });
    },
    closed: function (window) {
        if (!logoutEffect.isLogoutWindow(window)) {
            return;
        }
        // If the In animation is still active, retarget it so
        // the fade-out animation starts from the current opacity
        // and uses the appropriate duration to reach target opacity
        const duration = animationTime(200);
        if (window.inAnimation !== undefined) {
            if (retarget(window.inAnimation, 0.0, duration)) {
                return;
            }
            cancel(window.inAnimation);
            delete window.inAnimation;
        }
        window.outAnimation = animate({
            window: window,
            duration: duration,
            type: Effect.Opacity,
            from: 1.0,
            to: 0.0
        });
    },
    init: function () {
        effects.windowAdded.connect(logoutEffect.opened);
        effects.windowClosed.connect(logoutEffect.closed);
    }
};
logoutEffect.init();

