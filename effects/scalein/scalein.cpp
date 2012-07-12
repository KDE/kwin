/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "scalein.h"
#include <QtCore/QTimeLine>
#include <QtGui/QVector2D>

namespace KWin
{

KWIN_EFFECT(scalein, ScaleInEffect)

ScaleInEffect::ScaleInEffect()
    : Effect()
{
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
}

void ScaleInEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!mTimeLineWindows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data, time);
}

void ScaleInEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (mTimeLineWindows.contains(w)) {
        mTimeLineWindows[ w ]->setCurveShape(QTimeLine::EaseInOutCurve);
        mTimeLineWindows[ w ]->setCurrentTime(mTimeLineWindows[ w ]->currentTime() + time);
        if (mTimeLineWindows[ w ]->currentValue() < 1)
            data.setTransformed();
        else
            delete mTimeLineWindows.take(w);
    }
    effects->prePaintWindow(w, data, time);
}

void ScaleInEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (mTimeLineWindows.contains(w) && isScaleWindow(w)) {
        const qreal value = mTimeLineWindows[ w ]->currentValue();
        data.multiplyOpacity(value);
        data *= QVector2D(value, value);
        data += QPoint(int(w->width() / 2 * (1 - value)), int(w->height() / 2 * (1 - value)));
    }
    effects->paintWindow(w, mask, region, data);
}

bool ScaleInEffect::isScaleWindow(EffectWindow* w)
{
    const void* e = w->data(WindowAddedGrabRole).value<void*>();
    // TODO: isSpecialWindow is rather generic, maybe tell windowtypes separately?
    if (w->isPopupMenu() || w->isSpecialWindow() || w->isUtility() || (e && e != this))
        return false;
    return true;
}

void ScaleInEffect::postPaintWindow(EffectWindow* w)
{
    if (mTimeLineWindows.contains(w))
        w->addRepaintFull(); // trigger next animation repaint
    effects->postPaintWindow(w);
}

void ScaleInEffect::slotWindowAdded(EffectWindow* c)
{
    if (c->isOnCurrentDesktop()) {
        mTimeLineWindows.insert(c, new QTimeLine(animationTime(250), this));
        c->addRepaintFull();
    }
}

void ScaleInEffect::slotWindowClosed(EffectWindow* c)
{
    delete mTimeLineWindows.take(c);
}

bool ScaleInEffect::isActive() const
{
    return !mTimeLineWindows.isEmpty();
}

} // namespace
