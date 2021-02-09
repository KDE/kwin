/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var blacklist = [
    // The logout screen has to be animated only by the logout effect.
    "ksmserver ksmserver",
    "ksmserver-logout-greeter ksmserver-logout-greeter",

    // KDE Plasma splash screen has to be animated only by the login effect.
    "ksplashqml ksplashqml",
    "ksplashsimple ksplashsimple",
    "ksplashx ksplashx"
];

var scaleEffect = {
    loadConfig: function (window) {
        var defaultDuration = 160;
        var duration = effect.readConfig("Duration", defaultDuration) || defaultDuration;
        scaleEffect.duration = animationTime(duration);
        scaleEffect.inScale = effect.readConfig("InScale", 0.96);
        scaleEffect.inOpacity = effect.readConfig("InOpacity", 0.4);
        scaleEffect.outScale = effect.readConfig("OutScale", 0.96);
        scaleEffect.outOpacity = effect.readConfig("OutOpacity", 0.0);
    },
    isScaleWindow: function (window) {
        // We don't want to animate most of plasmashell's windows, yet, some
        // of them we want to, for example, Task Manager Settings window.
        // The problem is that all those window share single window class.
        // So, the only way to decide whether a window should be animated is
        // to use a heuristic: if a window has decoration, then it's most
        // likely a dialog or a settings window so we have to animate it.
        if (window.windowClass == "plasmashell plasmashell"
                || window.windowClass == "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }

        if (blacklist.indexOf(window.windowClass) != -1) {
            return false;
        }

        if (window.hasDecoration) {
            return true;
        }

        // Don't animate combobox popups, tooltips, popup menus, etc.
        if (window.popupWindow) {
            return false;
        }

        // Dont't animate the outline because it looks very sick.
        if (window.outline) {
            return false;
        }

        // Override-redirect windows are usually used for user interface
        // concepts that are not expected to be animated by this effect.
        if (window.x11Client && !window.managed) {
            return false;
        }

        return window.normalWindow || window.dialog;
    },
    setupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    },
    cleanupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    },
    slotWindowAdded: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!scaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible) {
            return;
        }
        if (!effect.grab(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        scaleEffect.setupForcedRoles(window);
        window.scaleInAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: scaleEffect.duration,
            animations: [
                {
                    type: Effect.Scale,
                    from: scaleEffect.inScale
                },
                {
                    type: Effect.Opacity,
                    from: scaleEffect.inOpacity
                }
            ]
        });
    },
    slotWindowClosed: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!scaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.scaleInAnimation) {
            cancel(window.scaleInAnimation);
            delete window.scaleInAnimation;
        }
        scaleEffect.setupForcedRoles(window);
        window.scaleOutAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: scaleEffect.duration,
            animations: [
                {
                    type: Effect.Scale,
                    to: scaleEffect.outScale
                },
                {
                    type: Effect.Opacity,
                    to: scaleEffect.outOpacity
                }
            ]
        });
    },
    slotWindowDataChanged: function (window, role) {
        if (role == Effect.WindowAddedGrabRole) {
            if (window.scaleInAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleInAnimation);
                delete window.scaleInAnimation;
                scaleEffect.cleanupForcedRoles(window);
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (window.scaleOutAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleOutAnimation);
                delete window.scaleOutAnimation;
                scaleEffect.cleanupForcedRoles(window);
            }
        }
    },
    init: function () {
        scaleEffect.loadConfig();

        effect.configChanged.connect(scaleEffect.loadConfig);
        effect.animationEnded.connect(scaleEffect.cleanupForcedRoles);
        effects.windowAdded.connect(scaleEffect.slotWindowAdded);
        effects.windowClosed.connect(scaleEffect.slotWindowClosed);
        effects.windowDataChanged.connect(scaleEffect.slotWindowDataChanged);
    }
};

scaleEffect.init();
