/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/*global effect, effects, Effect, QEasingCurve */
/*jslint continue: true */

var windowVibrateEffect = {
    duration: animationTime(30),
    offset: 10,
    repeat: 10,

    animate: function (w) {
        "use strict";
        var o = w.windowVibrate;
        var direction = o.countDown % 2 == windowVibrateEffect.repeat % 2;
        effect.animate({
            window: w,
            duration: windowVibrateEffect.duration,
            animations: [{
                type: Effect.Position,
                curve: QEasingCurve.InOutQuad,
                from: direction ? o.from : o.to,
                to: direction ? o.to : o.from,
            },],
        });
    },

    windowDataChanged: function (w, role) {
        "use strict";
        if (role !== Effect.WindowVibrateRole) {
            return;
        }
        var data = w.data(role);
        if (!data) {
            return;
        }

        var g = w.geometry;
        var x = g.x + g.width / 2;
        var y = g.y + g.height / 2;
        w.windowVibrate = {
            countDown: windowVibrateEffect.repeat,
            from: {
                value1: x - windowVibrateEffect.offset,
                value2: y,
            },
            to: {
                value1: x + windowVibrateEffect.offset,
                value2: y,
            },
        };
        windowVibrateEffect.animate(w);
    },

    animationEnded: function (w, animationId) {
        "use strict";
        if (!w.windowVibrate) {
            return;
        }
        
        w.windowVibrate.countDown -= 1;
        if (w.windowVibrate.countDown <= 0) {
            delete w.windowVibrate;
            w.setData(Effect.WindowVibrateRole, null);
            return;
        }

        windowVibrateEffect.animate(w);
    },

    init: function () {
        "use strict";
        effects.windowDataChanged.connect(windowVibrateEffect.windowDataChanged);
        effect.animationEnded.connect(windowVibrateEffect.animationEnded);
    }
};

windowVibrateEffect.init();
