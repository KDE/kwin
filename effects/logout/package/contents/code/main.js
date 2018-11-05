/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
 Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>
 Copyright (C) 2017 Marco Martin <mart@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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
        effects.windowShown.connect(logoutEffect.opened);
        effects.windowClosed.connect(logoutEffect.closed);
        effects.windowHidden.connect(logoutEffect.closed);
    }
};
logoutEffect.init();

