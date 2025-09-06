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
    animations: new Map(),
    loadConfig: function () {
        badBadWindowsEffect.duration = animationTime(250);
    },
    setShowingDesktop: function (showing) {
        badBadWindowsEffect.showingDesktop = showing;
    },
    showDesktop: function(frozenTime) {
        const stackingOrder = effects.stackingOrder;
        const screenGeometry = effects.virtualScreenGeometry;
        const xOffset = screenGeometry.width / 16;
        const yOffset = screenGeometry.height / 16;

        let closestWindows = [undefined, undefined, undefined, undefined];
        let movedWindowsCount = 0;
        let apertureCorner = new Map();
        let apertureDistances = new Map();

        for (const window of stackingOrder) {
            if (!window.hiddenByShowDesktop) {
                continue;
            }

            // ignore invisible windows and such that do not have to be restored
            if (!window.visible) {
                continue;
            }

            // Don't touch docks
            if (window.dock) {
                continue;
            }

            // calculate the corner distances
            const geo = window.geometry;
            const dl = geo.x + geo.width - screenGeometry.x;
            const dr = screenGeometry.x + screenGeometry.width - geo.x;
            const dt = geo.y + geo.height - screenGeometry.y;
            const db = screenGeometry.y + screenGeometry.height - geo.y;

            const distances = [dl + dt, dr + dt, dr + db, dl + db];
            apertureDistances.set(window, distances);
            movedWindowsCount += 1;

            // if this window is the closest one to any corner, set it as preferred there
            let nearest = 0;
            for (let j = 1; j < 4; ++j) {
                if (distances[j] < distances[nearest] ||
                    (distances[j] == distances[nearest] && closestWindows[j] === undefined)) {
                    nearest = j;
                }
            }
            if (closestWindows[nearest] === undefined ||
                apertureDistances.get(closestWindows[nearest])[nearest] > distances[nearest])
                closestWindows[nearest] = window;
        }

        // second pass, select corners

        // 1st off, move the nearest windows to their nearest corners
        // this will ensure that if there's only on window in the lower right
        // it won't be moved out to the upper left
        let movedWindowsDec = [ 0, 0, 0, 0 ];
        for (var i = 0; i < 4; ++i) {
            if (closestWindows[i] === undefined)
                continue;
            apertureCorner.set(closestWindows[i], i);
            apertureDistances.delete(closestWindows[i]);
            movedWindowsDec[i] = 1;
        }

        // 2nd, distribute the remainders according to their preferences
        // this doesn't exactly have heapsort performance ;-)
        movedWindowsCount = Math.floor((movedWindowsCount + 3) / 4);
        for (let i = 0; i < 4; ++i) {
            for (let j = 0; j < movedWindowsCount - movedWindowsDec[i]; ++j) {
                let bestWindow = undefined;
                for (let k = 0; k < stackingOrder.length; ++k) {
                    const candidateDistances = apertureDistances.get(stackingOrder[k]);
                    if (candidateDistances === undefined) {
                        continue;
                    }
                    if (bestWindow === undefined ||
                        candidateDistances[i] < apertureDistances.get(bestWindow)[i])
                        bestWindow = stackingOrder[k];
                }
                if (bestWindow === undefined)
                    break;
                apertureCorner.set(bestWindow, i);
                apertureDistances.delete(bestWindow);
            }
        }

        for (const window of stackingOrder) {
            if (window.dock) {
                continue;
            }

            const corner = apertureCorner.get(window);
            if (corner === undefined) {
                continue;
            }

            var anchor, tx, ty;
            if (corner == 1 || corner == 2) {
                tx = screenGeometry.x + screenGeometry.width - xOffset;
                anchor = Effect.Left;
            } else {
                tx = xOffset;
                anchor = Effect.Right;
            }
            if (corner > 1) {
                ty = screenGeometry.y + screenGeometry.height - yOffset;
                anchor |= Effect.Top;
            } else {
                ty = yOffset;
                anchor |= Effect.Bottom;
            }

            let animation = badBadWindowsEffect.animations.get(window);
            if (!animation) {
                animation = {};
            }

            let animationId = animation.id;
            if (!animationId || !freezeInTime(animationId, frozenTime)) {
                animationId = set({
                    window: window,
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

            badBadWindowsEffect.animations.set(window, {
                id: animationId,
                apertureCorner: corner,
            });
        }
    },
    hideDesktop: function(frozenTime) {
        const stackingOrder = effects.stackingOrder;
        const screenGeometry = effects.virtualScreenGeometry;
        const xOffset = screenGeometry.width / 16;
        const yOffset = screenGeometry.height / 16;

        for (const window of stackingOrder) {
            const animation = badBadWindowsEffect.animations.get(window);
            if (!animation) {
                continue;
            }

            let anchor, tx, ty;
            if (animation.apertureCorner == 1 || animation.apertureCorner == 2) {
                tx = screenGeometry.x + screenGeometry.width - xOffset;
                anchor = Effect.Left;
            } else {
                tx = xOffset;
                anchor = Effect.Right;
            }

            if (animation.apertureCorner > 1) {
                ty = screenGeometry.y + screenGeometry.height - yOffset;
                anchor |= Effect.Top;
            } else {
                ty = yOffset;
                anchor |= Effect.Bottom;
            }

            if (!window.visible) {
                cancel(animation.id);
            } else if (!redirect(animation.id, Effect.Backward) || !freezeInTime(animation.id, frozenTime)) {
                animate({
                    window: window,
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

            badBadWindowsEffect.animations.delete(window);
        }
    },
    showOrHideDesktop: function(frozenTime) {
        if (typeof frozenTime === "undefined") {
            frozenTime = -1;
        }

        if (badBadWindowsEffect.showingDesktop) {
            badBadWindowsEffect.showDesktop(frozenTime);
        } else {
            badBadWindowsEffect.hideDesktop(frozenTime);
        }
    },
    realtimeScreenEdgeCallback: function (border, deltaProgress, effectScreen) {
        if (!deltaProgress || !effectScreen) {
            badBadWindowsEffect.showOrHideDesktop();
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

        badBadWindowsEffect.showOrHideDesktop(time);
    },
    init: function () {
        badBadWindowsEffect.loadConfig();
        effects.showingDesktopChanged.connect(badBadWindowsEffect.setShowingDesktop);
        effects.showingDesktopChanged.connect(() => badBadWindowsEffect.showOrHideDesktop());
        effects.windowDeleted.connect(window => {
            badBadWindowsEffect.animations.delete(window);
        });

        let edges = effect.touchEdgesForAction("show-desktop");

        for (let i in edges) {
            let edge = parseInt(edges[i]);
            registerRealtimeScreenEdge(edge, badBadWindowsEffect.realtimeScreenEdgeCallback);
        }
    }
};

badBadWindowsEffect.init();
