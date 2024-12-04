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
        // If the Out animation is still active, kill it.
        if (window.outAnimation !== undefined) {
            cancel(window.outAnimation);
            delete window.outAnimation;
        }
        window.inAnimation = animate({
            window: window,
            duration: animationTime(800),
            type: Effect.Opacity,
            from: 0.0,
            to: 1.0
        });

        for (let window of effects.stackingOrder) {
            if (!window.visible) {
                continue;
            }

            if (window.desktopWindow || logoutEffect.isLogoutWindow(window)) {
                continue;
            }

            if (window.inAnimation) {
                cancel(window.inAnimation);
                window.inAnimation = undefined;
            }

            window.outAnimation = set({
                window: window,
                duration: animationTime(800),
                type: Effect.Opacity,
                to: 0.0
            });
        }
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
            duration: animationTime(400),
            type: Effect.Opacity,
            from: 1.0,
            to: 0.0
        });

        for (let window of effects.stackingOrder) {
            if (window.desktopWindow || logoutEffect.isLogoutWindow(window)) {
                continue;
            }

            if (window.outAnimation) {
                cancel(window.outAnimation);
                window.outAnimation = undefined;
            }

            if (!window.visible) {
                continue;
            }

            window.inAnimation = animate({
                window: window,
                duration: animationTime(200),
                type: Effect.Opacity,
                from: 0.0,
                to: 1.0
            });
        }
    },
    init: function () {
        effects.windowAdded.connect(logoutEffect.opened);
        effects.windowClosed.connect(logoutEffect.closed);
    }
};
logoutEffect.init();

