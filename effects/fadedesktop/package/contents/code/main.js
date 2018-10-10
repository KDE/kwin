/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
 Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
var duration;
function loadConfig() {
    duration = animationTime(250);
}
loadConfig();
effect.configChanged.connect(function() {
    loadConfig();
});
effects['desktopChanged(int,int)'].connect(function(oldDesktop, newDesktop) {
    if (effects.hasActiveFullScreenEffect && !effect.isActiveFullScreenEffect) {
        return;
    }
    var stackingOrder = effects.stackingOrder;
    for (var i = 0; i < stackingOrder.length; i++) {
        var w = stackingOrder[i];
        if (w.desktop != oldDesktop && w.desktop != newDesktop) {
            continue;
        }
        if (w.minimized) {
            continue;
        }
        if (!w.isOnActivity(effects.currentActivity)){
            continue;
        }
        if (w.desktop == oldDesktop) {
            animate({
                window: w,
                duration: duration,
                animations: [{
                    type: Effect.Opacity,
                    to: 0.0,
                    fullScreen: true
                }]
            });
        } else {
            animate({
                window: w,
                duration: duration,
                animations: [{
                    type: Effect.Opacity,
                    to: 1.0,
                    from: 0.0,
                    fullScreen: true
                }]
            });
        }
    }
});

effect.isActiveFullScreenEffectChanged.connect(function() {
    var isActiveFullScreen = effect.isActiveFullScreenEffect;
    var stackingOrder = effects.stackingOrder;
    for (var i = 0; i < stackingOrder.length; i++) {
        var w = stackingOrder[i];
        w.setData(Effect.WindowForceBlurRole, isActiveFullScreen);
        w.setData(Effect.WindowForceBackgroundContrastRole, isActiveFullScreen);
    }
});
