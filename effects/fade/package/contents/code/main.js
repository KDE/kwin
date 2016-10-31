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
function isFadeWindow(w) {
    if (w.deleted && effect.isGrabbed(w, Effect.WindowClosedGrabRole)) {
        return false;
    } else if (!w.deleted && effect.isGrabbed(w, Effect.WindowAddedGrabRole)) {
        return false;
    }
    return w.onCurrentDesktop && !isLoginWindow(w) && !w.desktopWindow && !w.utility && !w.minimized;
}

function isLoginWindow(w) {
    return w.windowClass == "ksplashx ksplashx" || w.windowClass == "ksplashsimple ksplashsimple" || w.windowClass == "ksplashqml ksplashqml";
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
effects.windowAdded.connect(function(w) {
    if (fadeWindows && isFadeWindow(w)) {
        w.fadeInWindowTypeAnimation = effect.animate(w, Effect.Opacity, fadeInTime, 1.0, 0.0);
    }
});
effects.windowClosed.connect(function(w) {
    if (fadeWindows && isFadeWindow(w)) {
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
});
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
