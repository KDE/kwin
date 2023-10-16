/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

class MaximizeEffect {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect(this.restoreForceBlurState.bind(this));

        effects.windowAdded.connect(this.manage.bind(this));
        for (const window of effects.stackingOrder) {
            this.manage(window);
        }

        this.loadConfig();
    }

    loadConfig() {
        this.duration = animationTime(250);
    }

    manage(window) {
        window.windowFrameGeometryChanged.connect(this.onWindowFrameGeometryChanged.bind(this));
        window.windowMaximizedStateChanged.connect(this.onWindowMaximizedStateChanged.bind(this));
        window.windowMaximizedStateAboutToChange.connect(this.onWindowMaximizedStateAboutToChange.bind(this));
    }

    onWindowMaximizedStateAboutToChange(window) {
        if (!window.visible) {
            return;
        }

        window.oldGeometry = Object.assign({}, window.geometry);

        if (window.maximizeAnimation1) {
            cancel(window.maximizeAnimation1);
            delete window.maximizeAnimation1;
        }
        let couldRetarget = false;
        if (window.maximizeAnimation2) {
            couldRetarget = retarget(window.maximizeAnimation2, 1.0, this.duration);
        }
        if (!couldRetarget) {
            window.maximizeAnimation2 = animate({
                window: window,
                duration: this.duration,
                animations: [{
                    type: Effect.CrossFadePrevious,
                    to: 1.0,
                    from: 0.0,
                    curve: QEasingCurve.OutCubic
                }]
            });
        }
    }

    onWindowMaximizedStateChanged(window) {
        if (!window.visible || !window.oldGeometry) {
            return;
        }
        window.setData(Effect.WindowForceBlurRole, true);
        const oldGeometry = window.oldGeometry;
        const newGeometry = window.geometry;
        window.maximizeAnimation1 = animate({
            window: window,
            duration: this.duration,
            animations: [{
                type: Effect.Size,
                to: {
                    value1: newGeometry.width,
                    value2: newGeometry.height
                },
                from: {
                    value1: oldGeometry.width,
                    value2: oldGeometry.height
                },
                curve: QEasingCurve.OutCubic
            }, {
                type: Effect.Translation,
                to: {
                    value1: 0,
                    value2: 0
                },
                from: {
                    value1: oldGeometry.x - newGeometry.x - (newGeometry.width / 2 - oldGeometry.width / 2),
                    value2: oldGeometry.y - newGeometry.y - (newGeometry.height / 2 - oldGeometry.height / 2)
                },
                curve: QEasingCurve.OutCubic
            }]
        });
    }

    restoreForceBlurState(window) {
        window.setData(Effect.WindowForceBlurRole, null);
    }

    onWindowFrameGeometryChanged(window, oldGeometry) {
        if (!window.maximizeAnimation1 ||
            // Check only dimension changes.
            (window.geometry.width == oldGeometry.width && window.geometry.height == oldGeometry.height) ||
            // Check only if last dimension isn't equal to dimension from which effect was started (window.oldGeometry).
            (window.oldGeometry.width == oldGeometry.width && window.oldGeometry.height == oldGeometry.height)
        ) {
            return;
        }

        // Cancel animation if window got resized halfway through it.
        cancel(window.maximizeAnimation1);
        delete window.maximizeAnimation1;

        if (window.maximizeAnimation2) {
            cancel(window.maximizeAnimation2);
            delete window.maximizeAnimation2;
        }
    }
}

new MaximizeEffect();
