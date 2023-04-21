/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var fadeDesktopEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        fadeDesktopEffect.duration = animationTime(250);
    },
    fadeInWindow: function (window) {
        if (window.fadeOutAnimation) {
            if (redirect(window.fadeOutAnimation, Effect.Backward)) {
                return;
            }
            cancel(window.fadeOutAnimation);
            delete window.fadeOutAnimation;
        }
        if (window.fadeInAnimation) {
            if (redirect(window.fadeInAnimation, Effect.Forward)) {
                return;
            }
            cancel(window.fadeInAnimation);
        }
        window.fadeInAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: fadeDesktopEffect.duration,
            fullScreen: true,
            keepAlive: false,
            type: Effect.Opacity,
            from: 0.0,
            to: 1.0
        });
    },
    fadeOutWindow: function (window) {
        if (window.fadeInAnimation) {
            if (redirect(window.fadeInAnimation, Effect.Backward)) {
                return;
            }
            cancel(window.fadeInAnimation);
            delete window.fadeInAnimation;
        }
        if (window.fadeOutAnimation) {
            if (redirect(window.fadeOutAnimation, Effect.Forward)) {
                return;
            }
            cancel(window.fadeOutAnimation);
        }
        window.fadeOutAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: fadeDesktopEffect.duration,
            fullScreen: true,
            keepAlive: false,
            type: Effect.Opacity,
            from: 1.0,
            to: 0.0
        });
    },
    slotDesktopChanged: function (oldDesktop, newDesktop, movingWindow) {
        if (effects.hasActiveFullScreenEffect && !effect.isActiveFullScreenEffect) {
            return;
        }

        var stackingOrder = effects.stackingOrder;
        for (var i = 0; i < stackingOrder.length; ++i) {
            var w = stackingOrder[i];

            // Don't animate windows that have been moved to the current
            // desktop, i.e. newDesktop.
            if (w == movingWindow) {
                continue;
            }

            // If the window is not on the old and the new desktop or it's
            // on both of them, then don't animate it.
            var onOldDesktop = w.isOnDesktop(oldDesktop);
            var onNewDesktop = w.isOnDesktop(newDesktop);
            if (onOldDesktop == onNewDesktop) {
                continue;
            }

            if (w.minimized) {
                continue;
            }

            if (!w.isOnActivity(effects.currentActivity)){
                continue;
            }

            if (onOldDesktop) {
                fadeDesktopEffect.fadeOutWindow(w);
            } else {
                fadeDesktopEffect.fadeInWindow(w);
            }
        }
    },
    slotIsActiveFullScreenEffectChanged: function () {
        var isActiveFullScreen = effect.isActiveFullScreenEffect;
        var stackingOrder = effects.stackingOrder;
        for (var i = 0; i < stackingOrder.length; ++i) {
            var w = stackingOrder[i];
            w.setData(Effect.WindowForceBlurRole, isActiveFullScreen);
            w.setData(Effect.WindowForceBackgroundContrastRole, isActiveFullScreen);
        }
    },
    init: function () {
        effect.configChanged.connect(fadeDesktopEffect.loadConfig);
        effect.isActiveFullScreenEffectChanged.connect(
            fadeDesktopEffect.slotIsActiveFullScreenEffectChanged);
        effects.desktopChanged.connect(fadeDesktopEffect.slotDesktopChanged);
    }
};

fadeDesktopEffect.init();
