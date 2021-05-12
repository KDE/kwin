/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018, 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

const blacklist = [
    // The logout screen has to be animated only by the logout effect.
    "ksmserver ksmserver",
    "ksmserver-logout-greeter ksmserver-logout-greeter",

    // KDE Plasma splash screen has to be animated only by the login effect.
    "ksplashqml ksplashqml"
];

class ScaleEffect {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect(this.cleanupForcedRoles.bind(this));
        effects.windowAdded.connect(this.slotWindowAdded.bind(this));
        effects.windowClosed.connect(this.slotWindowClosed.bind(this));
        effects.windowDataChanged.connect(this.slotWindowDataChanged.bind(this));

        this.loadConfig();
    }

    loadConfig() {
        const defaultDuration = 160;
        const duration = effect.readConfig("Duration", defaultDuration) || defaultDuration;
        this.duration = animationTime(duration);
        this.inScale = effect.readConfig("InScale", 0.96);
        this.inOpacity = effect.readConfig("InOpacity", 0.4);
        this.outScale = effect.readConfig("OutScale", 0.96);
        this.outOpacity = effect.readConfig("OutOpacity", 0.0);
    }

    static isScaleWindow(window) {
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

        // Dont't animate the outline and the screenlocker as it looks bad.
        if (window.lockScreen || window.outline) {
            return false;
        }

        // Override-redirect windows are usually used for user interface
        // concepts that are not expected to be animated by this effect.
        if (window.x11Client && !window.managed) {
            return false;
        }

        return window.normalWindow || window.dialog;
    }

    setupForcedRoles(window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    }

    cleanupForcedRoles(window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    }

    slotWindowAdded(window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!ScaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible) {
            return;
        }
        if (!effect.grab(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        this.setupForcedRoles(window);
        window.scaleInAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: this.duration,
            animations: [
                {
                    type: Effect.Scale,
                    from: this.inScale
                },
                {
                    type: Effect.Opacity,
                    from: this.inOpacity
                }
            ]
        });
    }

    slotWindowClosed(window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!ScaleEffect.isScaleWindow(window)) {
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
        this.setupForcedRoles(window);
        window.scaleOutAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: this.duration,
            animations: [
                {
                    type: Effect.Scale,
                    to: this.outScale
                },
                {
                    type: Effect.Opacity,
                    to: this.outOpacity
                }
            ]
        });
    }

    slotWindowDataChanged(window, role) {
        if (role == Effect.WindowAddedGrabRole) {
            if (window.scaleInAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleInAnimation);
                delete window.scaleInAnimation;
                this.cleanupForcedRoles(window);
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (window.scaleOutAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleOutAnimation);
                delete window.scaleOutAnimation;
                this.cleanupForcedRoles(window);
            }
        }
    }
}

new ScaleEffect();
