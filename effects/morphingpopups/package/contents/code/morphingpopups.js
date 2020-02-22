/********************************************************************
 This file is part of the KDE project.

 Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>
 Copyright (C) 2016 Marco Martin <mart@kde.org>

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

var morphingEffect = {
    duration: animationTime(150),
    loadConfig: function () {
        morphingEffect.duration = animationTime(150);
    },

    geometryChange: function (window, oldGeometry) {
        //only tooltips and notifications
        if (!window.tooltip && !window.notification && !window.criticalNotification) {
            return;
        }

        var newGeometry = window.geometry;

        //only do the transition for near enough tooltips,
        //don't cross the whole screen: ugly
        var distance = Math.abs(oldGeometry.x - newGeometry.x) + Math.abs(oldGeometry.y - newGeometry.y);

        if (newGeometry.x == oldGeometry.x &&
                newGeometry.y == oldGeometry.y &&
                newGeometry.width == oldGeometry.width &&
                newGeometry.height == oldGeometry.height) {
            return;
        }

        if (distance > (newGeometry.width + newGeometry.height) * 2) {
            if (window.moveAnimation) {
                delete window.moveAnimation;
            }
            if (window.fadeAnimation) {
                delete window.fadeAnimation;
            }

            return;
        }

        //don't resize it "too much", set as four times
        if ((newGeometry.width / oldGeometry.width) > 4 ||
            (oldGeometry.width / newGeometry.width) > 4 ||
            (newGeometry.height / oldGeometry.height) > 4 ||
            (oldGeometry.height / newGeometry.height) > 4) {
            return;
        }

        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);

        var couldRetarget = false;

        if (window.moveAnimation) {
            if (window.moveAnimation[0]) {
                couldRetarget = retarget(window.moveAnimation[0], {
                        value1: newGeometry.width,
                        value2: newGeometry.height
                    }, morphingEffect.duration);
            }
            if (couldRetarget && window.moveAnimation[1]) {
                couldRetarget = retarget(window.moveAnimation[1], {
                        value1: newGeometry.x + newGeometry.width/2,
                        value2: newGeometry.y + newGeometry.height / 2
                    }, morphingEffect.duration);
            }
            if (!couldRetarget) {
                cancel(window.moveAnimation[0]);
            }

        }

        if (!couldRetarget) {
            window.moveAnimation = animate({
                window: window,
                duration: morphingEffect.duration,
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
                    type: Effect.Position,
                    to: {
                        value1: newGeometry.x + newGeometry.width / 2,
                        value2: newGeometry.y + newGeometry.height / 2
                    },
                    from: {
                        value1: oldGeometry.x + oldGeometry.width / 2,
                        value2: oldGeometry.y + oldGeometry.height / 2
                    }
                }]
            });

        }

        couldRetarget = false;
        if (window.fadeAnimation) {
            couldRetarget = retarget(window.fadeAnimation[0], 1.0, morphingEffect.duration);
        }

        if (!couldRetarget) {
            window.fadeAnimation = animate({
                window: window,
                duration: morphingEffect.duration,
                animations: [{
                    type: Effect.CrossFadePrevious,
                    to: 1.0,
                    from: 0.0
                }]
            });
        }
    },

    init: function () {
        effect.configChanged.connect(morphingEffect.loadConfig);
        effects.windowGeometryShapeChanged.connect(morphingEffect.geometryChange);
    }
};
morphingEffect.init();
