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
    inDuration: animationTime(800),
    outDuration: animationTime(400),
    loadConfig: function () {
        logoutEffect.inDuration = animationTime(800);
        logoutEffect.outDuration = animationTime(400);
    },
    isLogoutWindow: function (window) {
        if (window.windowClass === "ksmserver ksmserver") {
            return true;
        }
        if (window.windowClass === "ksmserver-logout-greeter ksmserver-logout-greeter") {
            return true;
        }
        return false;
    },
    opened: function (window) {
        if (!logoutEffect.isLogoutWindow(window)) {
            return;
        }
        // If the Out animation is still active, kill it.
        if (window.outAnimation !== undefined) {
            cancel(window.outAnimation);
            delete window.outAnimation;
        }
        window.inAnimation = animate({
            window: window,
            duration: logoutEffect.inDuration,
            type: Effect.Opacity,
            from: 0.0,
            to: 1.0
        });
    },
    closed: function (window) {
        if (!logoutEffect.isLogoutWindow(window)) {
            return;
        }
        // If the In animation is still active, kill it.
        if (window.inAnimation !== undefined) {
            cancel(window.inAnimation);
            delete window.inAnimation;
        }
        window.outAnimation = animate({
            window: window,
            duration: logoutEffect.outDuration,
            type: Effect.Opacity,
            from: 1.0,
            to: 0.0
        });
    },
    init: function () {
        logoutEffect.loadConfig();
        effects.windowAdded.connect(logoutEffect.opened);
        effects.windowClosed.connect(logoutEffect.closed);
    }
};
logoutEffect.init();

