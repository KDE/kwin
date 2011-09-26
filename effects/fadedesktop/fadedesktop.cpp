/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "fadedesktop.h"

#include <math.h>

namespace KWin
{

KWIN_EFFECT(fadedesktop, FadeDesktopEffect)

FadeDesktopEffect::FadeDesktopEffect()
    : m_fading(false)
{
    connect(effects, SIGNAL(desktopChanged(int,int)), this, SLOT(slotDesktopChanged(int)));
    m_timeline.setCurveShape(QTimeLine::LinearCurve);
    reconfigure(ReconfigureAll);
}

void FadeDesktopEffect::reconfigure(ReconfigureFlags)
{
    m_timeline.setDuration(animationTime(250));
}

void FadeDesktopEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (m_fading) {
        m_timeline.setCurrentTime(m_timeline.currentTime() + time);

        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if (m_timeline.currentValue() != 1.0)
            data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        else {
            m_fading = false;
            m_timeline.setCurrentTime(0);
            foreach (EffectWindow * w, effects->stackingOrder()) {
                w->setData(WindowForceBlurRole, QVariant(false));
            }
            effects->setActiveFullScreenEffect(NULL);
        }
    }
    effects->prePaintScreen(data, time);
}

void FadeDesktopEffect::postPaintScreen()
{
    if (m_fading)
        effects->addRepaintFull();
    effects->postPaintScreen();
}

void FadeDesktopEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_fading) {
        if (w->isOnDesktop(m_oldDesktop))
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        data.setTranslucent();
    }
    effects->prePaintWindow(w, data, time);
}

void FadeDesktopEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (m_fading && !(w->isOnCurrentDesktop() && w->isOnDesktop(m_oldDesktop))) {
        if (w->isOnDesktop(m_oldDesktop))
            data.opacity *= 1 - m_timeline.currentValue();
        else
            data.opacity *= m_timeline.currentValue();
    }
    effects->paintWindow(w, mask, region, data);
}

void FadeDesktopEffect::slotDesktopChanged(int old)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    // TODO: Fix glitches when fading while a previous fade is still happening

    effects->setActiveFullScreenEffect(this);
    m_fading = true;
    m_timeline.setCurrentTime(0);
    m_oldDesktop = old;
    foreach (EffectWindow * w, effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->addRepaintFull();
}

bool FadeDesktopEffect::isActive() const
{
    return m_fading;
}

} // namespace

#include "fadedesktop.moc"
