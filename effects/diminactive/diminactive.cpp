/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "diminactive.h"

#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT(diminactive, DimInactiveEffect)

DimInactiveEffect::DimInactiveEffect()
{
    reconfigure(ReconfigureAll);
    timeline.setDuration(animationTime(250));
    previousActiveTimeline.setDuration(animationTime(250));
    active = effects->activeWindow();
    previousActive = NULL;
    connect(effects, SIGNAL(windowActivated(KWin::EffectWindow*)), this, SLOT(slotWindowActivated(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
}

void DimInactiveEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = EffectsHandler::effectConfig("DimInactive");
    dim_panels = conf.readEntry("DimPanels", false);
    dim_desktop = conf.readEntry("DimDesktop", false);
    dim_keepabove = conf.readEntry("DimKeepAbove", false);
    dim_by_group = conf.readEntry("DimByGroup", true);
    dim_strength = conf.readEntry("Strength", 25);
}

void DimInactiveEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    double oldValue = timeline.currentValue();
    if (effects->activeFullScreenEffect())
        timeline.setCurrentTime(timeline.currentTime() - time);
    else
        timeline.setCurrentTime(timeline.currentTime() + time);
    if (oldValue != timeline.currentValue())
        effects->addRepaintFull();
    if (previousActive) {
        // We are fading out the previous window
        previousActive->addRepaintFull();
        previousActiveTimeline.setCurrentTime(previousActiveTimeline.currentTime() + time);
    }
    effects->prePaintScreen(data, time);
}

void DimInactiveEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (dimWindow(w) || w == previousActive) {
        double previous = 1.0;
        if (w == previousActive)
            previous = previousActiveTimeline.currentValue();
        if (previousActiveTimeline.currentValue() == 1.0)
            previousActive = NULL;
        data.multiplyBrightness((1.0 - (dim_strength / 100.0) * timeline.currentValue() * previous));
        data.multiplySaturation((1.0 - (dim_strength / 100.0) * timeline.currentValue() * previous));
    }
    effects->paintWindow(w, mask, region, data);
}

bool DimInactiveEffect::dimWindow(const EffectWindow* w) const
{
    if (effects->activeWindow() == w)
        return false; // never dim active window
    if (active && dim_by_group && active->group() == w->group())
        return false; // don't dim in active group if configured so
    if (w->isDock() && !dim_panels)
        return false; // don't dim panels if configured so
    if (w->isDesktop() && !dim_desktop)
        return false; // don't dim the desktop if configured so
    if (w->keepAbove() && !dim_keepabove)
        return false; // don't dim keep-above windows if configured so
    if (!w->isNormalWindow() && !w->isDialog() && !w->isDock() && !w->isDesktop())
        return false; // don't dim more special window types
    // don't dim unmanaged windows, grouping doesn't work for them and maybe dimming
    // them doesn't make sense in general (they should be short-lived anyway)
    if (!w->isManaged())
        return false;
    return true; // dim the rest
}

void DimInactiveEffect::slotWindowDeleted(EffectWindow* w)
{
    if (w == previousActive)
        previousActive = NULL;
}

void DimInactiveEffect::slotWindowActivated(EffectWindow* w)
{
    if (active != NULL) {
        previousActive = active;
        previousActiveTimeline.setCurrentTime(0);
        if (!dimWindow(previousActive))
            previousActive = NULL;

        if (dim_by_group) {
            if ((w == NULL || w->group() != active->group()) && active->group() != NULL) {
                // repaint windows that are no longer in the active group
                foreach (EffectWindow * tmp, active->group()->members())
                tmp->addRepaintFull();
            }
        } else
            active->addRepaintFull();
    }
    active = w;
    if (active != NULL) {
        if (dim_by_group) {
            if (active->group() != NULL) {
                // repaint newly active windows
                foreach (EffectWindow * tmp, active->group()->members())
                tmp->addRepaintFull();
            }
        } else
            active->addRepaintFull();
    }
}

} // namespace
