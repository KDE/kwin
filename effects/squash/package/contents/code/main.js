/********************************************************************
 This file is part of the KDE project.

 Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

"use strict";

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
        var iconRect = window.iconGeometry;
        if (iconRect.width == 0 || iconRect.height == 0) {
            return;
        }

        if (window.unminimizeAnimation) {
            // TODO: Try to reverse it instead.
            cancel(window.unminimizeAnimation);
            delete window.unminimizeAnimation;
        }

        var windowRect = window.geometry;

        window.minimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: squashEffect.duration,
            animations: [
                {
                    type: Effect.Size,
                    from: {
                        value1: windowRect.width,
                        value2: windowRect.height
                    },
                    to: {
                        value1: iconRect.width,
                        value2: iconRect.height
                    }
                },
                {
                    type: Effect.Translation,
                    from: {
                        value1: 0.0,
                        value2: 0.0
                    },
                    to: {
                        value1: iconRect.x - windowRect.x -
                            (windowRect.width - iconRect.width) / 2,
                        value2: iconRect.y - windowRect.y -
                            (windowRect.height - iconRect.height) / 2,
                    }
                },
                {
                    type: Effect.Opacity,
                    from: 1.0,
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
        var iconRect = window.iconGeometry;
        if (iconRect.width == 0 || iconRect.height == 0) {
            return;
        }

        if (window.minimizeAnimation) {
            // TODO: Try to reverse it instead.
            cancel(window.minimizeAnimation);
            delete window.minimizeAnimation;
        }

        var windowRect = window.geometry;

        window.unminimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: squashEffect.duration,
            animations: [
                {
                    type: Effect.Size,
                    from: {
                        value1: iconRect.width,
                        value2: iconRect.height
                    },
                    to: {
                        value1: windowRect.width,
                        value2: windowRect.height
                    }
                },
                {
                    type: Effect.Translation,
                    from: {
                        value1: iconRect.x - windowRect.x -
                            (windowRect.width - iconRect.width) / 2,
                        value2: iconRect.y - windowRect.y -
                            (windowRect.height - iconRect.height) / 2,
                    },
                    to: {
                        value1: 0.0,
                        value2: 0.0
                    }
                },
                {
                    type: Effect.Opacity,
                    from: 0.0,
                    to: 1.0
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
