/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2015 Thomas Lübking <thomas.luebking@gmail.com>

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
/*global effect, effects, animate, animationTime, Effect, QEasingCurve */

var badBadWindowsEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        "use strict";
        badBadWindowsEffect.duration = animationTime(250);
    },
    offToCorners: function (showing) {
        "use strict";
        var stackingOrder = effects.stackingOrder;
        var screenGeo = effects.virtualScreenGeometry;
        var xOffset = screenGeo.width / 16;
        var yOffset = screenGeo.height / 16;
        for (var i = 0; i < stackingOrder.length; ++i) {
            var w = stackingOrder[i];

            // ignore windows above the desktop (when not showing, pretty much everything would be)
            if (w.desktopWindow && showing)
                break;

            // ignore invisible windows and such that do not have to be restored
            if (!w.visible) {
                if (!(showing || w.offToCornerId === undefined)) { // we still need to stop this
                    cancel(w.offToCornerId);
                    delete w.offToCornerId;
                    effects.setElevatedWindow(w, false);
                    if (!w.dock) {
                        animate({
                            window: w,
                            duration: badBadWindowsEffect.duration,
                            animations: [{
                                type: Effect.Opacity,
                                from: 0.2,
                                to: 0.0
                            }]
                        });
                    }
                }
                continue;
            }
            if (!showing && w.offToCornerId === undefined) {
                continue;
            }

            // keep windows above the desktop visually
            effects.setElevatedWindow(w, showing);

            // we just fade out docks - moving panels into edges looks dull
            if (w.dock) {
                if (showing) {
                    w.offToCornerId = set({
                        window: w,
                        duration: badBadWindowsEffect.duration,
                        animations: [{
                            type: Effect.Opacity,
                            to: 0.0
                        }]
                    });
                } else {
                    cancel(w.offToCornerId);
                    delete w.offToCornerId;
                    animate({
                        window: w,
                        duration: badBadWindowsEffect.duration,
                        animations: [{
                            type: Effect.Opacity,
                            from: 0.0
                        }]
                    });
                }
                continue; // ! ;-)
            }

            // calculate the closest corner
            var anchor, tx, ty;
            var geo = w.geometry;
            if (screenGeo.x + screenGeo.width - geo.x < geo.x + geo.width - screenGeo.x) {
                tx = screenGeo.x + screenGeo.width - xOffset;
                anchor = Effect.Left;
            } else {
                tx = xOffset;
                anchor = Effect.Right;
            }
            if (screenGeo.y + screenGeo.height - geo.y < geo.y + geo.height - screenGeo.y) {
                ty = screenGeo.y + screenGeo.height - yOffset;
                anchor |= Effect.Top;
            } else {
                ty = yOffset;
                anchor |= Effect.Bottom;
            }

            if (showing) {
                w.offToCornerId = set({
                    window: w,
                    duration: badBadWindowsEffect.duration,
                    curve: QEasingCurve.InOutQuad,
                    animations: [{
                        type: Effect.Position,
                        targetAnchor: anchor,
                        to: { value1: tx, value2: ty }
                    },{
                        type: Effect.Opacity,
                        to: 0.2
                    }]
                });
            } else {
                cancel(w.offToCornerId);
                delete w.offToCornerId;
                animate({
                    window: w,
                    duration: badBadWindowsEffect.duration,
                    curve: QEasingCurve.InOutQuad,
                    animations: [{
                        type: Effect.Position,
                        sourceAnchor: anchor,
                        from: { value1: tx, value2: ty }
                    },{
                        type: Effect.Opacity,
                        from: 0.2
                    }]
                });
            }
        }

    },
    init: function () {
        "use strict";
        badBadWindowsEffect.loadConfig();
        effects.showingDesktopChanged.connect(badBadWindowsEffect.offToCorners);
    }
};

badBadWindowsEffect.init();
