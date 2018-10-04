/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
 Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
    if (w.deleted && effect.isGrabbed(w, Effect.WindowClosedGrabRole)) {
        return false;
    } else if (!w.deleted && effect.isGrabbed(w, Effect.WindowAddedGrabRole)) {
        return false;
    }
    return w.onCurrentDesktop && !w.desktopWindow && !w.utility && !w.minimized;
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
effects.windowShown.connect(fadeInHandler);
effects.windowClosed.connect(fadeOutHandler);
effects.windowHidden.connect(fadeOutHandler);
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
