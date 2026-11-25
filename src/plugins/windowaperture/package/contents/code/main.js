/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Thomas Lübking <thomas.luebking@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/*global effect, effects, animate, animationTime, Effect, QEasingCurve */

"use strict";

var badBadWindowsEffect = {
    duration: animationTime(250),
    showingDesktop: false,
    loadConfig: function () {
        badBadWindowsEffect.duration = animationTime(250);
    },
    setShowingDesktop: function (showing) {
        badBadWindowsEffect.showingDesktop = showing;
    },
    offToCorners: function (showing, frozenTime) {
        if (typeof frozenTime === "undefined") {
            frozenTime = -1;
        }
        var stackingOrder = effects.stackingOrder;
        var screenGeo = effects.virtualScreenGeometry;
        var xOffset = screenGeo.width / 16;
        var yOffset = screenGeo.height / 16;
        if (showing) {
            for (var i = 0; i < stackingOrder.length; ++i) {
                var w = stackingOrder[i];

                if (!w.hiddenByShowDesktop) {
                    continue;
                }

                // ignore invisible windows and such that do not have to be restored
                if (!w.visible) {
                    if (w.offToCornerId) {
                        // if it was visible when the effect was activated delete its animation data
                        cancel(w.offToCornerId);
                        delete w.offToCornerId;
                        delete w.apertureCorner;
                    }
                    continue;
                }

                // Don't touch docks
                if (w.dock) {
                    continue;
                }

                // calculate the corner distances
                var geo = w.geometry;
                var dl = geo.x + geo.width - screenGeo.x;
                var dr = screenGeo.x + screenGeo.width - geo.x;
                var dt = geo.y + geo.height - screenGeo.y;
                var db = screenGeo.y + screenGeo.height - geo.y;
                var distances = [ dl + dt, dr + dt, dr + db, dl + db ];

                var nearest = 0;
                for (var j = 1; j < 4; ++j) {
                    if (distances[j] < distances[nearest]) {
                        nearest = j;
                    }
                }
                w.apertureCorner = nearest;
            }
        }

        // actually re/move windows from/to assigned corners
        for (var i = 0; i < stackingOrder.length; ++i) {
            var w = stackingOrder[i];
            if (w.apertureCorner === undefined && w.offToCornerId === undefined)
                continue;

            if (w.dock) {
                continue;
            }

            var anchor, tx, ty;
            var geo = w.geometry;
            if (w.apertureCorner == 1 || w.apertureCorner == 2) {
                tx = screenGeo.x + screenGeo.width - xOffset;
                anchor = Effect.Left;
            } else {
                tx = xOffset;
                anchor = Effect.Right;
            }
            if (w.apertureCorner > 1) {
                ty = screenGeo.y + screenGeo.height - yOffset;
                anchor |= Effect.Top;
            } else {
                ty = yOffset;
                anchor |= Effect.Bottom;
            }

            if (showing) {
                if (!w.offToCornerId || !freezeInTime(w.offToCornerId, frozenTime)) {

                    w.offToCornerId = set({
                        window: w,
                        duration: badBadWindowsEffect.duration,
                        curve: QEasingCurve.InOutCubic,
                        animations: [{
                            type: Effect.Position,
                            targetAnchor: anchor,
                            to: { value1: tx, value2: ty },
                            frozenTime: frozenTime
                        },{
                            type: Effect.Opacity,
                            to: 0.0,
                            frozenTime: frozenTime
                        }]
                    });
                }
            } else {
                // Reset if the window has become invisible in the meantime
                if (!w.visible) {
                    cancel(w.offToCornerId);
                    delete w.offToCornerId;
                    delete w.apertureCorner;
                // This if the window was invisible and has become visible in the meantime
                } else if (!w.offToCornerId || !redirect(w.offToCornerId, Effect.Backward) || !freezeInTime(w.offToCornerId, frozenTime)) {
                    animate({
                        window: w,
                        duration: badBadWindowsEffect.duration,
                        curve: QEasingCurve.InOutCubic,
                        animations: [{
                            type: Effect.Position,
                            sourceAnchor: anchor,
                            gesture: true,
                            from: { value1: tx, value2: ty }
                        },{
                            type: Effect.Opacity,
                            from: 0.0
                        }]
                    });
                }
            }
        }
    },
    animationEnded: function (w, a, meta) {
        // After the animation that closes the effect, reset all the parameters
        if (!badBadWindowsEffect.showingDesktop) {
            cancel(w.offToCornerId);
            delete w.offToCornerId;
            delete w.apertureCorner;
        }
    },
    realtimeScreenEdgeCallback: function (border, deltaProgress, effectScreen) {
        if (!deltaProgress || !effectScreen) {
            badBadWindowsEffect.offToCorners(badBadWindowsEffect.showingDesktop, -1);
            return;
        }
        let time = 0;

        switch (border) {
        case KWin.ElectricTop:
        case KWin.ElectricBottom:
            time = Math.min(1, (Math.abs(deltaProgress.height) / (effectScreen.geometry.height / 2))) * badBadWindowsEffect.duration;
            break;
        case KWin.ElectricLeft:
        case KWin.ElectricRight:
            time = Math.min(1, (Math.abs(deltaProgress.width) / (effectScreen.geometry.width / 2))) * badBadWindowsEffect.duration;
            break;
        default:
            return;
        }
        if (badBadWindowsEffect.showingDesktop) {
            time = badBadWindowsEffect.duration - time;
        }

        badBadWindowsEffect.offToCorners(true, time)
    },
    init: function () {
        badBadWindowsEffect.loadConfig();
        effect.configChanged.connect(badBadWindowsEffect.loadConfig);
        effects.showingDesktopChanged.connect(badBadWindowsEffect.setShowingDesktop);
        effects.showingDesktopChanged.connect(badBadWindowsEffect.offToCorners);
        effect.animationEnded.connect(badBadWindowsEffect.animationEnded);

        let edges = effect.touchEdgesForAction("show-desktop");

        for (let i in edges) {
            let edge = parseInt(edges[i]);
            registerRealtimeScreenEdge(edge, badBadWindowsEffect.realtimeScreenEdgeCallback);
        }
    }
};

badBadWindowsEffect.init();
