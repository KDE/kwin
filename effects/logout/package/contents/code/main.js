/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
 Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>
 Copyright (C) 2017 Marco Martin <mart@kde.org>

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
/*global effect, effects, animate, animationTime, Effect*/
var logoutEffect = {
    duration: animationTime(800),
    loadConfig: function () {
        "use strict";
        logoutEffect.duration = animationTime(800);
    },
    isLogoutWindow: function (window) {
        "use strict";
        var windowClass = window.windowClass;
        if (windowClass === "ksmserver ksmserver") {
            return true;
        }
        return false;
    },
    opened: function (window) {
        "use strict";
        if (!logoutEffect.isLogoutWindow(window)) {
            return;
        }
        animate({
            window: window,
            duration: logoutEffect.duration,
            type: Effect.Opacity,
            from: 0.0,
            to: 1.0
        });
    },
    init: function () {
        "use strict";
        logoutEffect.loadConfig();
        effects.windowAdded.connect(logoutEffect.opened);
        effects.windowShown.connect(logoutEffect.opened);
    }
};
logoutEffect.init();

