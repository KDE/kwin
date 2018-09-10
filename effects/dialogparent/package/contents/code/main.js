/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
/*global effect, effects, animate, cancel, set, animationTime, Effect, QEasingCurve */
/*jslint continue: true */
var dialogParentEffect = {
    duration: animationTime(300),
    windowAdded: function (window) {
        "use strict";
        if (window === null || window.modal === false) {
            return;
        }
        dialogParentEffect.dialogGotModality(window)
    },
    dialogGotModality: function (window) {
        "use strict";
        var mainWindows = window.mainWindows();
        for (var i = 0; i < mainWindows.length; i += 1) {
            var w = mainWindows[i];
            if (w.dialogParentAnimation !== undefined) {
                continue;
            }
            dialogParentEffect.startAnimation(w, dialogParentEffect.duration);
        }
    },
    startAnimation: function (window, duration) {
        "use strict";
        if (window.visible === false) {
            return;
        }
        window.dialogParentAnimation = set({
            window: window,
            duration: duration,
            animations: [{
                type: Effect.Saturation,
                to: 0.4
            }, {
                type: Effect.Brightness,
                to: 0.6
            }]
        });
    },
    windowClosed: function (window) {
        "use strict";
        dialogParentEffect.cancelAnimation(window);
        if (window.modal === false) {
            return;
        }
        dialogParentEffect.dialogLostModality(window);
    },
    dialogLostModality: function (window) {
        "use strict";
        var mainWindows = window.mainWindows();
        for (var i = 0; i < mainWindows.length; i += 1) {
            var w = mainWindows[i];
            if (w.dialogParentAnimation === undefined) {
                continue;
            }
            cancel(w.dialogParentAnimation);
            w.dialogParentAnimation = undefined;
            animate({
                window: w,
                duration: dialogParentEffect.duration,
                animations: [{
                    type: Effect.Saturation,
                    from: 0.4,
                    to: 1.0
                }, {
                    type: Effect.Brightness,
                    from: 0.6,
                    to: 1.0
                }]
            });
        }
    },
    cancelAnimation: function (window) {
        "use strict";
        if (window.dialogParentAnimation !== undefined) {
            cancel(window.dialogParentAnimation);
            window.dialogParentAnimation = undefined;
        }
    },
    desktopChanged: function () {
        "use strict";
        var i, windows, window;
        windows = effects.stackingOrder;
        for (i = 0; i < windows.length; i += 1) {
            window = windows[i];
            dialogParentEffect.cancelAnimation(window);
            dialogParentEffect.restartAnimation(window);
        }
    },
    modalDialogChanged: function(dialog) {
        "use strict";
        if (dialog.modal === false)
            dialogParentEffect.dialogLostModality(dialog);
        else if (dialog.modal === true)
            dialogParentEffect.dialogGotModality(dialog);
    },
    restartAnimation: function (window) {
        "use strict";
        if (window === null || window.findModal() === null) {
            return;
        }
        dialogParentEffect.startAnimation(window, 1);
    },
    init: function () {
        "use strict";
        var i, windows;
        effects.windowAdded.connect(dialogParentEffect.windowAdded);
        effects.windowClosed.connect(dialogParentEffect.windowClosed);
        effects.windowMinimized.connect(dialogParentEffect.cancelAnimation);
        effects.windowUnminimized.connect(dialogParentEffect.restartAnimation);
        effects.windowModalityChanged.connect(dialogParentEffect.modalDialogChanged)
        effects['desktopChanged(int,int)'].connect(dialogParentEffect.desktopChanged);
        effects.desktopPresenceChanged.connect(dialogParentEffect.cancelAnimation);
        effects.desktopPresenceChanged.connect(dialogParentEffect.restartAnimation);

        // start animation
        windows = effects.stackingOrder;
        for (i = 0; i < windows.length; i += 1) {
            dialogParentEffect.restartAnimation(windows[i]);
        }
    }
};
dialogParentEffect.init();
