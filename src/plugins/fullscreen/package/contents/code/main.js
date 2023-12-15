/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

class FullScreenEffect {
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
        window.windowFrameGeometryChanged.connect(
            this.onWindowFrameGeometryChanged.bind(this));
        window.windowFullScreenChanged.connect(
            this.onWindowFullScreenChanged.bind(this));
    }

    onWindowFullScreenChanged(window) {
        if (!window.visible || !window.oldGeometry) {
            return;
        }
        window.setData(Effect.WindowForceBlurRole, true);
        let oldGeometry = window.oldGeometry;
        const newGeometry = window.geometry;
        if (oldGeometry.width == newGeometry.width && oldGeometry.height == newGeometry.height)
            oldGeometry = window.olderGeometry;
        window.olderGeometry = Object.assign({}, window.oldGeometry);
        window.oldGeometry = Object.assign({}, newGeometry);

        let couldRetarget = false;
        if (window.fullScreenAnimation1) {
            if (window.fullScreenAnimation1[0]) {
                couldRetarget = retarget(window.fullScreenAnimation1[0], {
                    value1: newGeometry.width,
                    value2: newGeometry.height
                }, this.duration);
            }
            if (window.fullScreenAnimation1[1]) {
                couldRetarget = retarget(window.fullScreenAnimation1[1], {
                    value1: newGeometry.x + newGeometry.width / 2,
                    value2: newGeometry.y + newGeometry.height / 2
                }, this.duration);
            }
        }
        if (!couldRetarget) {
            if (window.fullScreenAnimation1) {
                cancel(window.fullScreenAnimation1);
                delete window.fullScreenAnimation1;
            }
            window.fullScreenAnimation1 = animate({
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
                    type: Effect.Position,
                    to: {
                        value1: newGeometry.x + newGeometry.width / 2,
                        value2: newGeometry.y + newGeometry.height / 2
                    },
                    from: {
                        value1: oldGeometry.x + oldGeometry.width / 2,
                        value2: oldGeometry.y + oldGeometry.height / 2
                    },
                    curve: QEasingCurve.OutCubic
                }]
            });
        }
        if (!window.resize) {
            window.fullScreenAnimation2 =animate({
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

    restoreForceBlurState(window) {
        window.setData(Effect.WindowForceBlurRole, null);
    }

    onWindowFrameGeometryChanged(window, oldGeometry) {
        if (window.fullScreenAnimation1) {
            if (window.geometry.width != window.oldGeometry.width ||
                window.geometry.height != window.oldGeometry.height) {
                if (window.fullScreenAnimation2) {
                    cancel(window.fullScreenAnimation2);
                    delete window.fullScreenAnimation2;
                }
            }
        }
        window.oldGeometry = Object.assign({}, window.geometry);
        window.olderGeometry = Object.assign({}, oldGeometry);
    }
}

new FullScreenEffect();
