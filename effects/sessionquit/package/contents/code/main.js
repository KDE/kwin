/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2019 David Edmundson <davidedmundson@kde.org>

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

var quitEffect = {
    closed: function (window) {
        if (!window.desktopWindow || effects.sessionState != Globals.Quitting) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        window.outAnimation = animate({
            window: window,
            duration: 30 * 1000, // 30 seconds should be long enough for any shutdown
            type: Effect.Generic, // do nothing, just hold a reference
            from: 0.0,
            to: 0.0
        });
    },
    init: function () {
        effects.windowClosed.connect(quitEffect.closed);
    }
};
quitEffect.init();
