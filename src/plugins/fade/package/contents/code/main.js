/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

    onWindowAdded(window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        if (!window.canBeAnimatedByToplevelOpenCloseEffect) {
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
        if (effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.skipsCloseAnimation || !window.canBeAnimatedByToplevelOpenCloseEffect) {
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
