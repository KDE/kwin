/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

const blacklist = [
    // The logout screen has to be animated only by the logout effect.
    "ksmserver ksmserver",
    "ksmserver-logout-greeter ksmserver-logout-greeter",

    // The splash screen has to be animated only by the login effect.
    "ksplashqml ksplashqml",

    // Spectacle needs to be blacklisted in order to stay out of its own screenshots.
    "spectacle spectacle", // x11
    "spectacle org.kde.spectacle", // wayland
];

class FadeEffect {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effects.windowAdded.connect(this.onWindowAdded.bind(this));
        effects.windowClosed.connect(this.onWindowClosed.bind(this));
        effects.windowDataChanged.connect(this.onWindowDataChanged.bind(this));

        this.loadConfig();
    }

    loadConfig() {
        this.fadeInTime = animationTime(effect.readConfig("FadeInTime", 150));
        this.fadeOutTime = animationTime(effect.readConfig("FadeOutTime", 150)) * 4;
    }

    static isFadeWindow(w) {
        if (blacklist.indexOf(w.windowClass) != -1) {
            return false;
        }
        if (w.popupWindow) {
            return false;
        }
        if (!w.managed) {
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

    onWindowAdded(window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!FadeEffect.isFadeWindow(window)) {
            return;
        }
        if (window.fadeOutAnimation) {
            cancel(window.fadeOutAnimation);
            delete window.fadeOutAnimation;
        }
        window.fadeInAnimation = effect.animate({
            window,
            duration: this.fadeInTime,
            type: Effect.Opacity,
            to: 1.0,
            from: 0.0
        });
    }

    onWindowClosed(window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (window.skipsCloseAnimation || !FadeEffect.isFadeWindow(window)) {
            return;
        }
        window.fadeOutAnimation = animate({
            window,
            duration: this.fadeOutTime,
            animations: [{
                type: Effect.Opacity,
                curve: QEasingCurve.OutQuart,
                to: 0.0
            }]
        });
    }

    onWindowDataChanged(window, role) {
        if (role == Effect.WindowAddedGrabRole) {
            if (effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
                if (window.fadeInAnimation) {
                    cancel(window.fadeInAnimation);
                    delete window.fadeInAnimation;
                }
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
                if (window.fadeOutAnimation) {
                    cancel(window.fadeOutAnimation);
                    delete window.fadeOutAnimation;
                }
            }
        }
    }
}

new FadeEffect();
