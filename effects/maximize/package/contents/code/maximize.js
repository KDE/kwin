/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var maximizeEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        maximizeEffect.duration = animationTime(250);
    },
    maximizeChanged: function (window) {
        if (!window.oldGeometry) {
            return;
        }
        window.setData(Effect.WindowForceBlurRole, true);
        var oldGeometry, newGeometry;
        oldGeometry = window.oldGeometry;
        newGeometry = window.geometry;
        if (oldGeometry.width == newGeometry.width && oldGeometry.height == newGeometry.height)
            oldGeometry = window.olderGeometry;
        window.olderGeometry = window.oldGeometry;
        window.oldGeometry = newGeometry;
        window.maximizeAnimation1 = animate({
            window: window,
            duration: maximizeEffect.duration,
            animations: [{
                type: Effect.Size,
                to: {
                    value1: newGeometry.width,
                    value2: newGeometry.height
                },
                from: {
                    value1: oldGeometry.width,
                    value2: oldGeometry.height
                }
            }, {
                type: Effect.Translation,
                to: {
                    value1: 0,
                    value2: 0
                },
                from: {
                    value1: oldGeometry.x - newGeometry.x - (newGeometry.width / 2 - oldGeometry.width / 2),
                    value2: oldGeometry.y - newGeometry.y - (newGeometry.height / 2 - oldGeometry.height / 2)
                }
            }]
        });
        if (!window.resize) {
            window.maximizeAnimation2 =animate({
                window: window,
                duration: maximizeEffect.duration,
                animations: [{
                    type: Effect.CrossFadePrevious,
                    to: 1.0,
                    from: 0.0
                }]
            });
        }
    },
    restoreForceBlurState: function(window) {
        window.setData(Effect.WindowForceBlurRole, null);
    },
    geometryChange: function (window, oldGeometry) {
        if (window.maximizeAnimation1) {
            if (window.geometry.width != window.oldGeometry.width ||
                window.geometry.height != window.oldGeometry.height) {
                cancel(window.maximizeAnimation1);
                delete window.maximizeAnimation1;
                if (window.maximizeAnimation2) {
                    cancel(window.maximizeAnimation2);
                    delete window.maximizeAnimation2;
                }
            }
        }
        window.oldGeometry = window.geometry;
        window.olderGeometry = oldGeometry;
    },
    init: function () {
        effect.configChanged.connect(maximizeEffect.loadConfig);
        effects.windowFrameGeometryChanged.connect(maximizeEffect.geometryChange);
        effects.windowMaximizedStateChanged.connect(maximizeEffect.maximizeChanged);
        effect.animationEnded.connect(maximizeEffect.restoreForceBlurState);
    }
};
maximizeEffect.init();
