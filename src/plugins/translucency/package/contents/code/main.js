/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/*global effect, effects, animate, cancel, set, animationTime, Effect, QEasingCurve */

"use strict";

var translucencyEffect = {
    activeWindow: effects.activeWindow,
    settings: {
        duration: animationTime(250),
        moveresize: 100,
        dialogs: 100,
        inactive: 100,
        comboboxpopups: 100,
        menus: 100,
        dropdownmenus: 100,
        popupmenus: 100,
        tornoffmenus: 100
    },
    loadConfig: function () {
        var i, individualMenu, windows;
        // TODO: add animation duration
        translucencyEffect.settings.moveresize     = effect.readConfig("MoveResize", 80);
        translucencyEffect.settings.dialogs        = effect.readConfig("Dialogs", 100);
        translucencyEffect.settings.inactive       = effect.readConfig("Inactive", 100);
        translucencyEffect.settings.comboboxpopups = effect.readConfig("ComboboxPopups", 100);
        translucencyEffect.settings.menus          = effect.readConfig("Menus", 100);
        individualMenu = effect.readConfig("IndividualMenuConfig", false);
        if (individualMenu === true) {
            translucencyEffect.settings.dropdownmenus = effect.readConfig("DropdownMenus", 100);
            translucencyEffect.settings.popupmenus    = effect.readConfig("PopupMenus", 100);
            translucencyEffect.settings.tornoffmenus  = effect.readConfig("TornOffMenus", 100);
        } else {
            translucencyEffect.settings.dropdownmenus = translucencyEffect.settings.menus;
            translucencyEffect.settings.popupmenus    = translucencyEffect.settings.menus;
            translucencyEffect.settings.tornoffmenus  = translucencyEffect.settings.menus;
        }

        windows = effects.stackingOrder;
        for (i = 0; i < windows.length; i += 1) {
            // stop all existing animations
            translucencyEffect.cancelAnimations(windows[i]);
            // schedule new animations based on new settings
            translucencyEffect.startAnimation(windows[i]);
            if (windows[i] !== effects.activeWindow) {
                translucencyEffect.inactive.animate(windows[i]);
            }
        }
    },
    /**
     * @brief Starts the set animations depending on window type
     *
     */
    startAnimation: function (window) {
        var checkWindow = function (window, value) {
            if (value !== 100) {
                var ids = set({
                    window: window,
                    duration: 1,
                    animations: [{
                        type: Effect.Opacity,
                        from: value / 100.0,
                        to: value / 100.0
                    }]
                });
                if (window.translucencyWindowTypeAnimation !== undefined) {
                    cancel(window.translucencyWindowTypeAnimation);
                }
                window.translucencyWindowTypeAnimation = ids;
            }
        };
        if (window.desktopWindow === true || window.dock === true || window.visible === false) {
            return;
        }
        if (window.dialog === true) {
            checkWindow(window, translucencyEffect.settings.dialogs);
        } else if (window.dropdownMenu === true) {
            checkWindow(window, translucencyEffect.settings.dropdownmenus);
        } else if (window.popupMenu === true) {
            checkWindow(window, translucencyEffect.settings.popupmenus);
        } else if (window.comboBox === true) {
            checkWindow(window, translucencyEffect.settings.comboboxpopups);
        } else if (window.menu === true) {
            checkWindow(window, translucencyEffect.settings.tornoffmenus);
        }
    },
    /**
     * @brief Cancels all animations for window type and inactive window
     *
     */
    cancelAnimations: function (window) {
        if (window.translucencyWindowTypeAnimation !== undefined) {
            cancel(window.translucencyWindowTypeAnimation);
            window.translucencyWindowTypeAnimation = undefined;
        }
        if (window.translucencyInactiveAnimation !== undefined) {
            cancel(window.translucencyInactiveAnimation);
            window.translucencyInactiveAnimation = undefined;
        }
        if (window.translucencyMoveResizeAnimations !== undefined) {
            cancel(window.translucencyMoveResizeAnimations);
            window.translucencyMoveResizeAnimations = undefined;
        }
    },
    moveResize: {
        start: function (window) {
            var ids;
            if (translucencyEffect.settings.moveresize === 100) {
                return;
            }
            ids = set({
                window: window,
                duration: translucencyEffect.settings.duration,
                animations: [{
                    type: Effect.Opacity,
                    to: translucencyEffect.settings.moveresize / 100.0
                }]
            });
            window.translucencyMoveResizeAnimations = ids;
        },
        finish: function (window) {
            if (window.translucencyMoveResizeAnimations !== undefined) {
                // start revert animation
                animate({
                    window: window,
                    duration: translucencyEffect.settings.duration,
                    animations: [{
                        type: Effect.Opacity,
                        from: translucencyEffect.settings.moveresize / 100.0
                    }]
                });
                // and cancel previous animation
                cancel(window.translucencyMoveResizeAnimations);
                window.translucencyMoveResizeAnimations = undefined;
            }
        }
    },
    inactive: {
        activated: function (window) {
            if (translucencyEffect.settings.inactive === 100) {
                return;
            }
            translucencyEffect.inactive.animate(translucencyEffect.activeWindow);
            translucencyEffect.activeWindow = window;
            if (window === null) {
                return;
            }
            if (window.translucencyInactiveAnimation !== undefined) {
                // start revert animation
                animate({
                    window: window,
                    duration: translucencyEffect.settings.duration,
                    animations: [{
                        type: Effect.Opacity,
                        from: translucencyEffect.settings.inactive / 100.0
                    }]
                });
                // and cancel previous animation
                cancel(window.translucencyInactiveAnimation);
                window.translucencyInactiveAnimation = undefined;
            }
        },
        animate: function (window) {
            var ids;
            if (translucencyEffect.settings.inactive === 100) {
                return;
            }
            if (window === null) {
                return;
            }
            if (window === effects.activeWindow ||
                    window.popup === true ||
                    window.managed === false ||
                    window.desktopWindow === true ||
                    window.dock === true ||
                    window.visible === false ||
                    window.deleted === true) {
                return;
            }
            ids = set({
                window: window,
                duration: translucencyEffect.settings.duration,
                animations: [{
                    type: Effect.Opacity,
                    to: translucencyEffect.settings.inactive / 100.0
                }]
            });
            window.translucencyInactiveAnimation = ids;
        }
    },
    desktopChanged: function () {
        var i, windows;
        windows = effects.stackingOrder;
        for (i = 0; i < windows.length; i += 1) {
            translucencyEffect.cancelAnimations(windows[i]);
            translucencyEffect.startAnimation(windows[i]);
            if (windows[i] !== effects.activeWindow) {
                translucencyEffect.inactive.animate(windows[i]);
            }
        }
    },
    manage: function (window) {
        window.windowDesktopsChanged.connect(translucencyEffect.cancelAnimations);
        window.windowDesktopsChanged.connect(translucencyEffect.startAnimation);
        window.windowStartUserMovedResized.connect(translucencyEffect.moveResize.start);
        window.windowFinishUserMovedResized.connect(translucencyEffect.moveResize.finish);

        window.minimizedChanged.connect(() => {
            if (window.minimized) {
                translucencyEffect.cancelAnimations();
            } else {
                translucencyEffect.startAnimation(window);
                translucencyEffect.inactive.animate(window);
            }
        });
    },
    init: function () {
        effect.configChanged.connect(translucencyEffect.loadConfig);
        effects.windowAdded.connect(translucencyEffect.manage);
        effects.windowAdded.connect(translucencyEffect.startAnimation);
        effects.windowClosed.connect(translucencyEffect.cancelAnimations);
        effects.windowActivated.connect(translucencyEffect.inactive.activated);
        effects.desktopChanged.connect(translucencyEffect.desktopChanged);

        for (const window of effects.stackingOrder) {
            translucencyEffect.manage(window);
        }

        translucencyEffect.loadConfig();
    }
};
translucencyEffect.init();
