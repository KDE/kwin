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

#include <QVector2D>

namespace KWin
{

MinimizeAnimationEffect::MinimizeAnimationEffect()
{
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowDeleted, this, &MinimizeAnimationEffect::windowDeleted);
    connect(effects, &EffectsHandler::windowMinimized, this, &MinimizeAnimationEffect::windowMinimized);
    connect(effects, &EffectsHandler::windowUnminimized, this, &MinimizeAnimationEffect::windowUnminimized);
}

void MinimizeAnimationEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    m_duration = std::chrono::milliseconds(static_cast<int>(animationTime(250)));
}

bool MinimizeAnimationEffect::supported()
{
    return effects->animationsSupported();
}

void MinimizeAnimationEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    const std::chrono::milliseconds delta(time);

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).update(delta);
        ++animationIt;
    }

    // We need to mark the screen windows as transformed. Otherwise the
    // whole screen won't be repainted, resulting in artefacts.
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
}

void MinimizeAnimationEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_animations.contains(w)) {
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }

    effects->prePaintWindow(w, data, time);
}

void MinimizeAnimationEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    const auto animationIt = m_animations.constFind(w);
    if (animationIt != m_animations.constEnd()) {
        // 0 = not minimized, 1 = fully minimized
        const qreal progress = (*animationIt).value();

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        // If there's no icon geometry, minimize to the center of the screen
        if (!icon.isValid()) {
            icon = QRect(effects->virtualScreenGeometry().center(), QSize(0, 0));
        }

        data *= QVector2D(interpolate(1.0, icon.width() / (double)geo.width(), progress),
                          interpolate(1.0, icon.height() / (double)geo.height(), progress));
        data.setXTranslation(interpolate(data.xTranslation(), icon.x() - geo.x(), progress));
        data.setYTranslation(interpolate(data.yTranslation(), icon.y() - geo.y(), progress));
        data.multiplyOpacity(interpolate(1.0, 0.1, progress));
    }

    effects->paintWindow(w, mask, region, data);
}

void MinimizeAnimationEffect::postPaintScreen()
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        if ((*animationIt).done()) {
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->addRepaintFull();

    effects->postPaintScreen();
}

void MinimizeAnimationEffect::windowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

void MinimizeAnimationEffect::windowMinimized(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    TimeLine &timeLine = m_animations[w];

    if (timeLine.running()) {
        timeLine.toggleDirection();
    } else {
        timeLine.setDirection(TimeLine::Forward);
        timeLine.setDuration(m_duration);
        timeLine.setEasingCurve(QEasingCurve::InOutSine);
    }

    effects->addRepaintFull();
}

void MinimizeAnimationEffect::windowUnminimized(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    TimeLine &timeLine = m_animations[w];

    if (timeLine.running()) {
        timeLine.toggleDirection();
    } else {
        timeLine.setDirection(TimeLine::Backward);
        timeLine.setDuration(m_duration);
        timeLine.setEasingCurve(QEasingCurve::InOutSine);
    }

    effects->addRepaintFull();
}

bool MinimizeAnimationEffect::isActive() const
{
    return !m_animations.isEmpty();
}

} // namespace

