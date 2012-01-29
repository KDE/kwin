/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2011 Thomas Lübking <thomas.luebking@web.de>

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

#include "dialogparent.h"

namespace KWin
{

KWIN_EFFECT(dialogparent, DialogParentEffect)

DialogParentEffect::DialogParentEffect()
{
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowActivated(KWin::EffectWindow*)), this, SLOT(slotWindowActivated(KWin::EffectWindow*)));
}

void DialogParentEffect::reconfigure(ReconfigureFlags)
{
    // How long does it take for the effect to get it's full strength (in ms)
    changeTime = animationTime(300);
}

void DialogParentEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    QMap<EffectWindow*, float>::iterator it = effectStrength.find(w);
    if (it != effectStrength.end()) {
        if (!w->findModal()) {
            *it -= time/changeTime;
            if (*it <= 0.0f)
                effectStrength.erase(it);
        }
        else if (*it < 1.0f)
            *it = qMin(1.0f, *it + time/changeTime);
    }
    effects->prePaintWindow(w, data, time);
}

void DialogParentEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    const float s = effectStrength.value(w, 0.0);
    if (s > 0.0f) {
        data.brightness *= (1.0f - 0.4*s); // [1.0; 0.6]
        data.saturation *= (1.0f - 0.6*s); // [1.0; 0.4]
    }
    effects->paintWindow(w, mask, region, data);
}

void DialogParentEffect::postPaintWindow(EffectWindow* w)
{
    const float s = effectStrength.value(w, 0.0);
    if (s > 0.0f && s < 1.0f) // not done yet
        w->addRepaintFull();

    effects->postPaintWindow( w );
}

void DialogParentEffect::slotWindowActivated(EffectWindow* w)
{
    if (w && w->isModal()) {
        EffectWindowList mainwindows = w->mainWindows();
        foreach (EffectWindow* parent, mainwindows) {
            if (!effectStrength.contains(parent))
                effectStrength[parent] = 0.0;
            parent->addRepaintFull();
        }
    }
}

void DialogParentEffect::slotWindowClosed(EffectWindow* w)
{
    if (w && w->isModal()) {
        EffectWindowList mainwindows = w->mainWindows();
        foreach(EffectWindow* parent, mainwindows)
            parent->addRepaintFull(); // brighten up
    }
    effectStrength.remove(w);
}

bool DialogParentEffect::isActive() const
{
    return !effectStrength.isEmpty();
}

} // namespace
