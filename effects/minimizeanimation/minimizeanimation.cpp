/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "minimizeanimation.h"
#include <QTimeLine>
#include <QtGui/QVector2D>

namespace KWin
{

MinimizeAnimationEffect::MinimizeAnimationEffect()
{
    mActiveAnimations = 0;
    connect(effects, &EffectsHandler::windowDeleted, this, &MinimizeAnimationEffect::windowDeleted);
    connect(effects, &EffectsHandler::windowMinimized, this, &MinimizeAnimationEffect::windowMinimized);
    connect(effects, &EffectsHandler::windowUnminimized, this, &MinimizeAnimationEffect::windowUnminimized);
}

bool MinimizeAnimationEffect::supported()
{
    return effects->animationsSupported();
}

void MinimizeAnimationEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{

    QHash< EffectWindow*, QTimeLine* >::iterator entry = mTimeLineWindows.begin();
    bool erase = false;
    while (entry != mTimeLineWindows.end()) {
        QTimeLine *timeline = entry.value();
        if (entry.key()->isMinimized()) {
            timeline->setCurrentTime(timeline->currentTime() + time);
            erase = (timeline->currentValue() >= 1.0f);
        } else {
            timeline->setCurrentTime(timeline->currentTime() - time);
            erase = (timeline->currentValue() <= 0.0f);
        }
        if (erase) {
            delete timeline;
            entry = mTimeLineWindows.erase(entry);
        } else
            ++entry;
    }

    mActiveAnimations = mTimeLineWindows.count();
    if (mActiveAnimations > 0)
        // We need to mark the screen windows as transformed. Otherwise the
        //  whole screen won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
}

void MinimizeAnimationEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // Schedule window for transformation if the animation is still in
    //  progress
    if (mTimeLineWindows.contains(w)) {
        // We'll transform this window
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }

    effects->prePaintWindow(w, data, time);
}

void MinimizeAnimationEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    QHash< EffectWindow*, QTimeLine* >::const_iterator entry = mTimeLineWindows.constFind(w);
    if (entry != mTimeLineWindows.constEnd()) {
        // 0 = not minimized, 1 = fully minimized
        double progress = entry.value()->currentValue();

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        // If there's no icon geometry, minimize to the center of the screen
        if (!icon.isValid())
            icon = QRect(effects->virtualScreenGeometry().center(), QSize(0, 0));

        data *= QVector2D(interpolate(1.0, icon.width() / (double)geo.width(), progress),
                          interpolate(1.0, icon.height() / (double)geo.height(), progress));
        data.setXTranslation((int)interpolate(data.xTranslation(), icon.x() - geo.x(), progress));
        data.setYTranslation((int)interpolate(data.yTranslation(), icon.y() - geo.y(), progress));
        data.multiplyOpacity(0.1 + (1 - progress) * 0.9);
    }

    // Call the next effect.
    effects->paintWindow(w, mask, region, data);
}

void MinimizeAnimationEffect::postPaintScreen()
{
    if (mActiveAnimations > 0)
        // Repaint the workspace so that everything would be repainted next time
        effects->addRepaintFull();
    mActiveAnimations = mTimeLineWindows.count();

    // Call the next effect.
    effects->postPaintScreen();
}

void MinimizeAnimationEffect::windowDeleted(EffectWindow *w)
{
    delete mTimeLineWindows.take(w);
}

void MinimizeAnimationEffect::windowMinimized(EffectWindow *w)
{
    if (effects->activeFullScreenEffect())
        return;

    if (!mTimeLineWindows.contains(w)) {
        auto *timeline = new QTimeLine(animationTime(250), this);
        timeline->setCurrentTime(0);
        timeline->setCurveShape(QTimeLine::EaseInOutCurve);
        mTimeLineWindows.insert(w, timeline);
    }
}

void MinimizeAnimationEffect::windowUnminimized(EffectWindow *w)
{
    if (effects->activeFullScreenEffect())
        return;

    if (!mTimeLineWindows.contains(w)) {
        auto *timeline = new QTimeLine(animationTime(250), this);
        timeline->setCurrentTime(timeline->duration());
        timeline->setCurveShape(QTimeLine::EaseInOutCurve);
        mTimeLineWindows.insert(w, timeline);
    }
}

bool MinimizeAnimationEffect::isActive() const
{
    return !mTimeLineWindows.isEmpty();
}

} // namespace

