/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Thomas Lübking <thomas.luebking@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/*global effect, effects, animate, animationTime, Effect, QEasingCurve */

"use strict";

var eyeOnScreenEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        eyeOnScreenEffect.duration = animationTime(250);
    },
    slurp: function (showing) {
        var stackingOrder = effects.stackingOrder;
        var screenGeo = effects.virtualScreenGeometry;
        var center = { value1: screenGeo.x + screenGeo.width/2,
                       value2: screenGeo.y + screenGeo.height/2 };
        for (var i = 0; i < stackingOrder.length; ++i) {
            var w = stackingOrder[i];
            if (!w.visible || !(showing || w.slurpedByEyeOnScreen)) {
                continue;
            }
            w.slurpedByEyeOnScreen = showing;
            if (w.desktopWindow) {
                // causes "seizures" because of opposing movements
                // var zoom = showing ? 0.8 : 1.2;
                var zoom = 0.8;
                w.eyeOnScreenShowsDesktop = showing;
                animate({
                    window: w,
                    duration: 2*eyeOnScreenEffect.duration, // "*2 for "bumper" transition
                    animations: [{
                        type: Effect.Scale,
                        curve: Effect.GaussianCurve,
                        to: zoom
                    }, {
                        type: Effect.Opacity,
                        curve: Effect.GaussianCurve,
                        to: 0.0
                    }]
                });
            } else {
                if (showing) {
                    animate({
                        window: w,
                        animations: [{
                            type: Effect.Scale,
                            curve: QEasingCurve.InCubic,
                            duration: eyeOnScreenEffect.duration,
                            to: 0.0
                        }, {
                            type: Effect.Position,
                            curve: QEasingCurve.InCubic,
                            duration: eyeOnScreenEffect.duration,
                            to: center
                        }, {
                            type: Effect.Opacity,
                            curve: QEasingCurve.InCubic,
                            duration: eyeOnScreenEffect.duration,
                            to: 0.0
                        }]
                    });
                } else {
                    animate({
                        window: w,
                        duration: eyeOnScreenEffect.duration,
                        delay: eyeOnScreenEffect.duration,
                        animations: [{
                            type: Effect.Scale,
                            curve: QEasingCurve.OutCubic,
                            from: 0.0
                        }, {
                            type: Effect.Position,
                            curve: QEasingCurve.OutCubic,
                            from: center
                        }, {
                            type: Effect.Opacity,
                            curve: QEasingCurve.OutCubic,
                            from: 0.0
                        }]
                    });
                }
            }
        }
    },
    init: function () {
        eyeOnScreenEffect.loadConfig();
        effects.showingDesktopChanged.connect(eyeOnScreenEffect.slurp);
    }
};

eyeOnScreenEffect.init();
