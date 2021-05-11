/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

function fitScale(rect, bounds)
{
    if (bounds.width > bounds.height) {
        return bounds.height / rect.height;
    } else {
        return bounds.width / rect.width;
    }
}

var squashEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        squashEffect.duration = animationTime(250);
    },
    slotWindowMinimized: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }

        // If the window doesn't have an icon in the task manager,
        // don't animate it.
        const iconRect = window.iconGeometry;
        if (iconRect.width == 0 || iconRect.height == 0) {
            return;
        }

        const windowRect = window.geometry;
        if (windowRect.width == 0 || windowRect.height == 0) {
            return;
        }

        if (window.unminimizeAnimation) {
            if (redirect(window.unminimizeAnimation, Effect.Backward)) {
                return;
            }
            cancel(window.unminimizeAnimation);
            delete window.unminimizeAnimation;
        }

        if (window.minimizeAnimation) {
            if (redirect(window.minimizeAnimation, Effect.Forward)) {
                return;
            }
            cancel(window.minimizeAnimation);
        }

        window.minimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.InCubic,
            duration: squashEffect.duration,
            animations: [
                {
                    type: Effect.Scale,
                    to: fitScale(windowRect, iconRect)
                },
                {
                    type: Effect.Position,
                    to: {
                        value1: iconRect.x + iconRect.width / 2,
                        value2: iconRect.y + iconRect.height / 2
                    }
                },
                {
                    type: Effect.Opacity,
                    to: 0.0
                }
            ]
        });
    },
    slotWindowUnminimized: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }

        // If the window doesn't have an icon in the task manager,
        // don't animate it.
        const iconRect = window.iconGeometry;
        if (iconRect.width == 0 || iconRect.height == 0) {
            return;
        }

        const windowRect = window.geometry;
        if (windowRect.width == 0 || windowRect.height == 0) {
            return;
        }

        if (window.minimizeAnimation) {
            if (redirect(window.minimizeAnimation, Effect.Backward)) {
                return;
            }
            cancel(window.minimizeAnimation);
            delete window.minimizeAnimation;
        }

        if (window.unminimizeAnimation) {
            if (redirect(window.unminimizeAnimation, Effect.Forward)) {
                return;
            }
            cancel(window.unminimizeAnimation);
        }

        window.unminimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.OutCubic,
            duration: squashEffect.duration,
            animations: [
                {
                    type: Effect.Scale,
                    from: fitScale(windowRect, iconRect)
                },
                {
                    type: Effect.Position,
                    from: {
                        value1: iconRect.x + iconRect.width / 2,
                        value2: iconRect.y + iconRect.height / 2
                    }
                },
                {
                    type: Effect.Opacity,
                    from: 0.0
                }
            ]
        });
    },
    init: function () {
        effect.configChanged.connect(squashEffect.loadConfig);
        effects.windowMinimized.connect(squashEffect.slotWindowMinimized);
        effects.windowUnminimized.connect(squashEffect.slotWindowUnminimized);
    }
};

squashEffect.init();
