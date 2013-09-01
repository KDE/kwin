/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2013 Kai Uwe Broulik <kde@privat.broulik.de>

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
var scaleInEffect = {
    duration: animationTime(250),
    loadConfig: function () {
        "use strict";
        scaleInEffect.duration = animationTime(250);
    },
    isScaleWindow: function (window) {
        "use strict";
        if (window.popupMenu || window.specialWindow || window.utility || window.minimized ||
                effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            return false;
        }
        return true;
    },
    scaleIn: function (window) {
        "use strict";
        animate({
            window: window,
            duration: scaleInEffect.duration,
            curve: QEasingCurve.InOutQuad,
            animations: [{
                type: Effect.Opacity,
                from: 0.0,
                to: 1.0
            }, {
                type: Effect.Scale,
                from: 0.0,
                to: 1.0
            }]
        });
    },
    added: function (window) {
        "use strict";
        if (!scaleInEffect.isScaleWindow(window)) {
            return;
        }
        scaleInEffect.scaleIn(window);
    },
    init: function () {
        "use strict";
        effect.configChanged.connect(scaleInEffect.loadConfig);
        effects.windowAdded.connect(scaleInEffect.added);
    }
};
scaleInEffect.init();
