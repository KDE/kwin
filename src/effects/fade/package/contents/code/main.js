/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

var blacklist = [
    // The logout screen has to be animated only by the logout effect.
    "ksmserver ksmserver",
    "ksmserver-logout-greeter ksmserver-logout-greeter",

    // The splash screen has to be animated only by the login effect.
    "ksplashqml ksplashqml",
    "ksplashsimple ksplashsimple",
    "ksplashx ksplashx"
];

function isFadeWindow(w) {
    if (blacklist.indexOf(w.windowClass) != -1) {
        return false;
    }
    if (w.popupWindow) {
        return false;
    }
    if (w.x11Client && !w.managed) {
        return false;
    }
    if (!w.visible) {
        return false;
    }
    if (w.outline) {
        return false;
    }
    if (w.deleted && effect.isGrabbed(w, Effect.WindowClosedGrabRole)) {
        return false;
    } else if (!w.deleted && effect.isGrabbed(w, Effect.WindowAddedGrabRole)) {
        return false;
    }
    return w.normalWindow || w.dialog;
}

var fadeInTime, fadeOutTime, fadeWindows;
function loadConfig() {
    fadeInTime = animationTime(effect.readConfig("FadeInTime", 150));
    fadeOutTime = animationTime(effect.readConfig("FadeOutTime", 150)) * 4;
    fadeWindows = effect.readConfig("FadeWindows", true);
}
loadConfig();
effect.configChanged.connect(function() {
    loadConfig();
});
function fadeInHandler(w) {
    if (effects.hasActiveFullScreenEffect) {
        return;
    }
    if (fadeWindows && isFadeWindow(w)) {
        if (w.fadeOutWindowTypeAnimation !== undefined) {
            cancel(w.fadeOutWindowTypeAnimation);
            w.fadeOutWindowTypeAnimation = undefined;
        }
        w.fadeInWindowTypeAnimation = effect.animate(w, Effect.Opacity, fadeInTime, 1.0, 0.0);
    }
}
function fadeOutHandler(w) {
    if (effects.hasActiveFullScreenEffect) {
        return;
    }
    if (fadeWindows && isFadeWindow(w)) {
        if (w.fadeOutWindowTypeAnimation !== undefined) {
            // don't animate again as it was already animated through window hidden
            return;
        }
        w.fadeOutWindowTypeAnimation = animate({
            window: w,
            duration: fadeOutTime,
            animations: [{
                type: Effect.Opacity,
                curve: QEasingCurve.OutQuart,
                to: 0.0
            }]
        });
    }
}
effects.windowAdded.connect(fadeInHandler);
effects.windowClosed.connect(fadeOutHandler);
effects.windowDataChanged.connect(function (window, role) {
    if (role == Effect.WindowAddedGrabRole) {
        if (effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            if (window.fadeInWindowTypeAnimation !== undefined) {
                cancel(window.fadeInWindowTypeAnimation);
                window.fadeInWindowTypeAnimation = undefined;
            }
        }
    } else if (role == Effect.WindowClosedGrabRole) {
        if (effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
            if (window.fadeOutWindowTypeAnimation !== undefined) {
                cancel(window.fadeOutWindowTypeAnimation);
                window.fadeOutWindowTypeAnimation = undefined;
            }
        }
    }
});
