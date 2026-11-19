/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var squashEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        squashEffect.duration = animationTime(250);
    },
    iconGeometry: function (window) {
        var iconRect = window.iconGeometry;
        if (iconRect.width != 0 && iconRect.height != 0) {
            return iconRect;
        }

        var windowRect = window.geometry;
        return {
            x: windowRect.x + windowRect.width / 2,
            y: windowRect.y + windowRect.height / 2,
            width: 0,
            height: 0
        };
    },
    slotAnimationEnded: function (window, animationId) {
        if (window.minimizeAnimation) {
            const index = window.minimizeAnimation.indexOf(animationId);
            if (index > -1) {
                window.minimizeAnimation.splice(index, 1);

                if (window.minimizeAnimation.length === 0) {
                    delete window.minimizeAnimation;
                } else {
                    return;
                }
            }
        }

        if (window.unminimizeAnimation) {
            const index = window.unminimizeAnimation.indexOf(animationId);
            if (index > -1) {
                window.unminimizeAnimation.splice(index, 1);

                if (window.unminimizeAnimation.length === 0) {
                    delete window.unminimizeAnimation;
                } else {
                    return;
                }
            }
        }

        window.windowFrameGeometryChanged.disconnect(squashEffect.slotWindowFrameGeometryChanged);

        window.setData(Effect.WindowForceBlurRole, null);
    },
    slotWindowMinimized: function (window) {
        if (effects.hasActiveFullScreenEffect) {
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

        window.setData(Effect.WindowForceBlurRole, true);

        const iconRect = squashEffect.iconGeometry(window);
        const windowRect = window.geometry;

        window.minimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.InCubic,
            duration: squashEffect.duration,
            keepAlive: false,
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

        window.windowFrameGeometryChanged.connect(squashEffect.slotWindowFrameGeometryChanged);
    },
    slotWindowUnminimized: function (window) {
        if (effects.hasActiveFullScreenEffect) {
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

        window.setData(Effect.WindowForceBlurRole, true);

        const iconRect = squashEffect.iconGeometry(window);
        const windowRect = window.geometry;

        window.unminimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.OutCubic,
            duration: squashEffect.duration,
            keepAlive: false,
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

        window.windowFrameGeometryChanged.connect(squashEffect.slotWindowFrameGeometryChanged);
    },
    slotWindowFrameGeometryChanged: function (window) {
        const iconRect = squashEffect.iconGeometry(window);
        const windowRect = window.geometry;

        if (window.minimizeAnimation) {
            // The window changed its geometry while being animated to the panel.

            retarget(window.minimizeAnimation[0], {
                value1: iconRect.width,
                value2: iconRect.height
            });

            retarget(window.minimizeAnimation[1], {
                value1: iconRect.x - windowRect.x -
                    (windowRect.width - iconRect.width) / 2,
                value2: iconRect.y - windowRect.y -
                    (windowRect.height - iconRect.height) / 2,
            });
        } else if (window.unminimizeAnimation) {
            // The window changed its geometry while being animated from the panel.

            retarget(window.unminimizeAnimation[0], {
                value1: windowRect.width,
                value2: windowRect.height
            });

            retarget(window.unminimizeAnimation[1], {
                value1: 0.0,
                value2: 0.0
            });
        }
    },
    slotWindowAdded: function (window) {
        window.minimizedChanged.connect(() => {
            if (window.minimized) {
                squashEffect.slotWindowMinimized(window);
            } else {
                squashEffect.slotWindowUnminimized(window);
            }
        });
    },
    init: function () {
        effect.configChanged.connect(squashEffect.loadConfig);
        effect.animationEnded.connect(squashEffect.slotAnimationEnded);

        effects.windowAdded.connect(squashEffect.slotWindowAdded);
        for (const window of effects.stackingOrder) {
            squashEffect.slotWindowAdded(window);
        }
    }
};

squashEffect.init();
