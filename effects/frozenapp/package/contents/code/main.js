/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var frozenAppEffect = {
    inDuration: animationTime(1500),
    outDuration: animationTime(250),
    loadConfig: function () {
        frozenAppEffect.inDuration = animationTime(1500);
        frozenAppEffect.outDuration = animationTime(250);
    },
    windowAdded: function (window) {
        if (!window || !window.unresponsive) {
            return;
        }
        frozenAppEffect.windowBecameUnresponsive(window);
    },
    windowBecameUnresponsive: function (window) {
        if (window.unresponsiveAnimation) {
            return;
        }
        frozenAppEffect.startAnimation(window, frozenAppEffect.inDuration);
    },
    startAnimation: function (window, duration) {
        if (!window.visible) {
            return;
        }
        window.unresponsiveAnimation = set({
            window: window,
            duration: duration,
            animations: [{
                type: Effect.Saturation,
                to: 0.1
            }, {
                type: Effect.Brightness,
                to: 1.5
            }]
        });
    },
    windowClosed: function (window) {
        frozenAppEffect.cancelAnimation(window);
        if (!window.unresponsive) {
            return;
        }
        frozenAppEffect.windowBecameResponsive(window);
    },
    windowBecameResponsive: function (window) {
        if (!window.unresponsiveAnimation) {
            return;
        }
        cancel(window.unresponsiveAnimation);
        window.unresponsiveAnimation = undefined;

        animate({
            window: window,
            duration: frozenAppEffect.outDuration,
            animations: [{
                type: Effect.Saturation,
                from: 0.1,
                to: 1.0
            }, {
                type: Effect.Brightness,
                from: 1.5,
                to: 1.0
            }]
        });
    },
    cancelAnimation: function (window) {
        if (window.unresponsiveAnimation) {
            cancel(window.unresponsiveAnimation);
            window.unresponsiveAnimation = undefined;
        }
    },
    desktopChanged: function () {
        var windows = effects.stackingOrder;
        for (var i = 0, length = windows.length; i < length; ++i) {
            var window = windows[i];
            frozenAppEffect.cancelAnimation(window);
            frozenAppEffect.restartAnimation(window);
        }
    },
    unresponsiveChanged: function (window) {
        if (window.unresponsive) {
            frozenAppEffect.windowBecameUnresponsive(window);
        } else {
            frozenAppEffect.windowBecameResponsive(window);
        }
    },
    restartAnimation: function (window) {
        if (!window || !window.unresponsive) {
            return;
        }
        frozenAppEffect.startAnimation(window, 1);
    },
    init: function () {
        effects.windowAdded.connect(frozenAppEffect.windowAdded);
        effects.windowClosed.connect(frozenAppEffect.windowClosed);
        effects.windowMinimized.connect(frozenAppEffect.cancelAnimation);
        effects.windowUnminimized.connect(frozenAppEffect.restartAnimation);
        effects.windowUnresponsiveChanged.connect(frozenAppEffect.unresponsiveChanged);
        effects['desktopChanged(int,int)'].connect(frozenAppEffect.desktopChanged);
        effects.desktopPresenceChanged.connect(frozenAppEffect.cancelAnimation);
        effects.desktopPresenceChanged.connect(frozenAppEffect.restartAnimation);

        var windows = effects.stackingOrder;
        for (var i = 0, length = windows.length; i < length; ++i) {
            frozenAppEffect.restartAnimation(windows[i]);
        }
    }
};
frozenAppEffect.init();
