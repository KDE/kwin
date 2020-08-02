/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/*global effect, effects, animate, cancel, set, animationTime, Effect, QEasingCurve */
/*jslint continue: true */
var dialogParentEffect = {
    duration: animationTime(300),
    windowAdded: function (window) {
        "use strict";
        if (window.modal) {
            dialogParentEffect.dialogGotModality(window);
        }
    },
    dialogGotModality: function (window) {
        "use strict";
        var mainWindows = window.mainWindows();
        for (var i = 0; i < mainWindows.length; ++i) {
            dialogParentEffect.startAnimation(mainWindows[i]);
        }
    },
    startAnimation: function (window) {
        "use strict";
        if (window.visible === false) {
            return;
        }
        if (window.dialogParentAnimation) {
            if (redirect(window.dialogParentAnimation, Effect.Forward)) {
                return;
            }
            cancel(window.dialogParentAnimation);
        }
        window.dialogParentAnimation = set({
            window: window,
            duration: dialogParentEffect.duration,
            keepAlive: false,
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
        if (window.modal) {
            dialogParentEffect.dialogLostModality(window);
        }
    },
    dialogLostModality: function (window) {
        "use strict";
        var mainWindows = window.mainWindows();
        for (var i = 0; i < mainWindows.length; ++i) {
            dialogParentEffect.cancelAnimationSmooth(mainWindows[i]);
        }
    },
    cancelAnimationInstant: function (window) {
        "use strict";
        if (window.dialogParentAnimation) {
            cancel(window.dialogParentAnimation);
            delete window.dialogParentAnimation;
        }
    },
    cancelAnimationSmooth: function (window) {
        "use strict";
        if (!window.dialogParentAnimation) {
            return;
        }
        if (redirect(window.dialogParentAnimation, Effect.Backward)) {
            return;
        }
        cancel(window.dialogParentAnimation);
        delete window.dialogParentEffect;
    },
    desktopChanged: function () {
        "use strict";
        // If there is an active full screen effect, then try smoothly dim/brighten
        // the main windows. Keep in mind that in order for this to work properly, this
        // effect has to come after the full screen effect in the effect chain,
        // otherwise this slot will be invoked before the full screen effect can mark
        // itself as a full screen effect.
        if (effects.hasActiveFullScreenEffect) {
            return;
        }

        var windows = effects.stackingOrder;
        for (var i = 0; i < windows.length; ++i) {
            var window = windows[i];
            dialogParentEffect.cancelAnimationInstant(window);
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
        dialogParentEffect.startAnimation(window);
        if (window.dialogParentAnimation) {
            complete(window.dialogParentAnimation);
        }
    },
    activeFullScreenEffectChanged: function () {
        "use strict";
        var windows = effects.stackingOrder;
        for (var i = 0; i < windows.length; ++i) {
            var dialog = windows[i];
            if (!dialog.modal) {
                continue;
            }
            if (effects.hasActiveFullScreenEffect) {
                dialogParentEffect.dialogLostModality(dialog);
            } else {
                dialogParentEffect.dialogGotModality(dialog);
            }
        }
    },
    init: function () {
        "use strict";
        var i, windows;
        effects.windowAdded.connect(dialogParentEffect.windowAdded);
        effects.windowClosed.connect(dialogParentEffect.windowClosed);
        effects.windowMinimized.connect(dialogParentEffect.cancelAnimationInstant);
        effects.windowUnminimized.connect(dialogParentEffect.restartAnimation);
        effects.windowModalityChanged.connect(dialogParentEffect.modalDialogChanged)
        effects['desktopChanged(int,int)'].connect(dialogParentEffect.desktopChanged);
        effects.desktopPresenceChanged.connect(dialogParentEffect.cancelAnimationInstant);
        effects.desktopPresenceChanged.connect(dialogParentEffect.restartAnimation);
        effects.activeFullScreenEffectChanged.connect(
            dialogParentEffect.activeFullScreenEffectChanged);

        // start animation
        windows = effects.stackingOrder;
        for (i = 0; i < windows.length; i += 1) {
            dialogParentEffect.restartAnimation(windows[i]);
        }
    }
};
dialogParentEffect.init();
