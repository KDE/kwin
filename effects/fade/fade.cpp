/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

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

#include "fade.h"

#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT(fade, FadeEffect)

FadeEffect::FadeEffect()
{
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowAdded(EffectWindow*)), this, SLOT(slotWindowAdded(EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(EffectWindow*)), this, SLOT(slotWindowClosed(EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(EffectWindow*)), this, SLOT(slotWindowDeleted(EffectWindow*)));
    connect(effects, SIGNAL(windowOpacityChanged(EffectWindow*,qreal,qreal)), this, SLOT(slotWindowOpacityChanged(EffectWindow*,qreal)));
}

void FadeEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("Fade");
    fadeInTime = animationTime(conf, "FadeInTime", 150);
    fadeOutTime = animationTime(conf, "FadeOutTime", 150);
    fadeWindows = conf.readEntry("FadeWindows", true);

    // Add all existing windows to the window list
    // TODO: Enabling desktop effects should trigger windowAdded() on all windows
    windows.clear();
    if (!fadeWindows)
        return;
}

void FadeEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!windows.isEmpty()) {
        fadeInStep = time / double(fadeInTime);
        fadeOutStep = time / double(fadeOutTime);
    }
    effects->prePaintScreen(data, time);
}

void FadeEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (windows.contains(w)) {
        windows[ w ].fadeInStep += fadeInStep;
        windows[ w ].fadeOutStep += fadeOutStep;
        if (windows[ w ].opacity < 1.0)
            data.setTranslucent();
        if (windows[ w ].deleted) {
            if (windows[ w ].opacity <= 0.0) {
                windows.remove(w);
                w->unrefWindow();
            } else
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
        }
    }
    effects->prePaintWindow(w, data, time);
    if (windows.contains(w) && !w->isPaintingEnabled() && !effects->activeFullScreenEffect()) {
        // if the window isn't to be painted, then let's make sure
        // to track its progress
        if (windows[ w ].fadeInStep < 1.0
                || windows[ w ].fadeOutStep < 1.0) {
            // but only if the total change is less than the
            // maximum possible change
            w->addRepaintFull();
        } else {
            if (windows[w].deleted) {
                w->unrefWindow();
            }
            windows.remove(w);
        }
    }
}

void FadeEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (windows.contains(w)) {
        if (windows[ w ].deleted
                || windows[ w ].opacity != data.opacity
                || windows[ w ].saturation != data.saturation
                || windows[ w ].brightness != data.brightness) {
            WindowPaintData new_data = data;

            if (windows[ w ].deleted)
                new_data.opacity = 0.0;

            if (new_data.opacity > windows[ w ].opacity) {
                windows[ w ].opacity = qMin(new_data.opacity,
                                            windows[ w ].opacity + windows[ w ].fadeInStep);
            } else if (new_data.opacity < windows[ w ].opacity) {
                windows[ w ].opacity = qMax(new_data.opacity,
                                            windows[ w ].opacity - windows[ w ].fadeOutStep);
            }

            if (new_data.saturation > windows[ w ].saturation) {
                windows[ w ].saturation = qMin(new_data.saturation,
                                               windows[ w ].saturation + windows[ w ].fadeInStep);
            } else if (new_data.saturation < windows[ w ].saturation) {
                windows[ w ].saturation = qMax(new_data.saturation,
                                               windows[ w ].saturation - windows[ w ].fadeOutStep);
            }

            if (new_data.brightness > windows[ w ].brightness) {
                windows[ w ].brightness = qMin(new_data.brightness,
                                               windows[ w ].brightness + windows[ w ].fadeInStep);
            } else if (new_data.brightness < windows[ w ].brightness) {
                windows[ w ].brightness = qMax(new_data.brightness,
                                               windows[ w ].brightness - windows[ w ].fadeOutStep);
            }

            windows[ w ].opacity = qBound(0.0, windows[ w ].opacity, 1.0);
            windows[ w ].saturation = qBound(0.0, windows[ w ].saturation, 1.0);
            windows[ w ].brightness = qBound(0.0, windows[ w ].brightness, 1.0);
            windows[ w ].fadeInStep = 0.0;
            windows[ w ].fadeOutStep = 0.0;

            new_data.opacity = windows[ w ].opacity;
            new_data.saturation = windows[ w ].saturation;
            new_data.brightness = windows[ w ].brightness;
            effects->paintWindow(w, mask, region, new_data);
            if (windows[ w ].opacity != data.opacity
                    || windows[ w ].saturation != data.saturation
                    || windows[ w ].brightness != data.brightness)
                w->addRepaintFull();
            return;
        } else {
            windows.remove(w);
        }
    }
    effects->paintWindow(w, mask, region, data);
}

void FadeEffect::slotWindowOpacityChanged(EffectWindow* w, qreal old_opacity)
{
    if (!windows.contains(w) || !isFadeWindow(w))
        return;
    if (windows[ w ].opacity == 1.0)
        windows[ w ].opacity -= 0.1 / fadeOutTime;
    w->addRepaintFull();
}

void FadeEffect::slotWindowAdded(EffectWindow* w)
{
    if (!fadeWindows || !isFadeWindow(w))
        return;
    windows[ w ].opacity = 0.0;
    w->addRepaintFull();
}

void FadeEffect::slotWindowClosed(EffectWindow* w)
{
    if (!fadeWindows || !isFadeWindow(w))
        return;
    if (!windows.contains(w))
        windows[ w ].opacity = w->opacity();
    if (windows[ w ].opacity == 1.0)
        windows[ w ].opacity -= 0.1 / fadeOutTime;
    windows[ w ].deleted = true;
    w->refWindow();
    w->addRepaintFull();
}

void FadeEffect::slotWindowDeleted(EffectWindow* w)
{
    windows.remove(w);
}

bool FadeEffect::isFadeWindow(EffectWindow* w)
{
    void* e;
    if (w->isDeleted())
        e = w->data(WindowClosedGrabRole).value<void*>();
    else
        e = w->data(WindowAddedGrabRole).value<void*>();
    if (w->windowClass() == "ksplashx ksplashx"
            || w->windowClass() == "ksplashsimple ksplashsimple"
            || (e && e != this)) {
        // see login effect
        return false;
    }
    return (!w->isDesktop() && !w->isUtility());
}

bool FadeEffect::isActive() const
{
    return !windows.isEmpty();
}

} // namespace
