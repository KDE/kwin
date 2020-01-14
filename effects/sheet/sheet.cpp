/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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

// own
#include "sheet.h"

// KConfigSkeleton
#include "sheetconfig.h"

// Qt
#include <QMatrix4x4>

namespace KWin
{

SheetEffect::SheetEffect()
{
    initConfig<SheetConfig>();
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &SheetEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SheetEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &SheetEffect::slotWindowDeleted);
}

void SheetEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    SheetConfig::self()->read();

    // TODO: Rename AnimationTime config key to Duration.
    const int d = animationTime(SheetConfig::animationTime() != 0
        ? SheetConfig::animationTime()
        : 300);
    m_duration = std::chrono::milliseconds(static_cast<int>(d));
}

void SheetEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    const std::chrono::milliseconds delta(time);

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).timeLine.update(delta);
        ++animationIt;
    }

    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
}

void SheetEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_animations.contains(w)) {
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
    }

    effects->prePaintWindow(w, data, time);
}

void SheetEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt == m_animations.constEnd()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    // Perspective projection distorts objects near edges of the viewport
    // in undesired way. To fix this, the center of the window will be
    // moved to the origin, after applying perspective projection, the
    // center is moved back to its "original" projected position. Overall,
    // this is how the window will be transformed:
    //  [move to the origin] -> [scale] -> [rotate] -> [translate] ->
    //    -> [perspective projection] -> [reverse "move to the origin"]
    const QMatrix4x4 oldProjMatrix = data.screenProjectionMatrix();
    const QRectF windowGeo = w->geometry();
    const QVector3D invOffset = oldProjMatrix.map(QVector3D(windowGeo.center()));
    QMatrix4x4 invOffsetMatrix;
    invOffsetMatrix.translate(invOffset.x(), invOffset.y());
    data.setProjectionMatrix(invOffsetMatrix * oldProjMatrix);

    // Move the center of the window to the origin.
    const QRectF screenGeo = effects->virtualScreenGeometry();
    const QPointF offset = screenGeo.center() - windowGeo.center();
    data.translate(offset.x(), offset.y());

    const qreal t = (*animationIt).timeLine.value();
    data.setRotationAxis(Qt::XAxis);
    data.setRotationAngle(interpolate(60.0, 0.0, t));
    data *= QVector3D(1.0, t, t);
    data.translate(0.0, -interpolate(w->y() - (*animationIt).parentY, 0.0, t));

    data.multiplyOpacity(t);

    effects->paintWindow(w, mask, region, data);
}

void SheetEffect::postPaintWindow(EffectWindow *w)
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        EffectWindow *w = animationIt.key();
        w->addRepaintFull();
        if ((*animationIt).timeLine.done()) {
            if (w->isDeleted()) {
                w->unrefWindow();
            }
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    if (m_animations.isEmpty()) {
        effects->addRepaintFull();
    }

    effects->postPaintWindow(w);
}

bool SheetEffect::isActive() const
{
    return !m_animations.isEmpty();
}

bool SheetEffect::supported()
{
    return effects->isOpenGLCompositing()
        && effects->animationsSupported();
}

void SheetEffect::slotWindowAdded(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isSheetWindow(w)) {
        return;
    }

    Animation &animation = m_animations[w];
    animation.parentY = 0;
    animation.timeLine.reset();
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setEasingCurve(QEasingCurve::Linear);

    const auto windows = effects->stackingOrder();
    auto parentIt = std::find_if(windows.constBegin(), windows.constEnd(),
        [w](EffectWindow *p) {
            return p->findModal() == w;
        });
    if (parentIt != windows.constEnd()) {
        animation.parentY = (*parentIt)->y();
    }

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    w->addRepaintFull();
}

void SheetEffect::slotWindowClosed(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isSheetWindow(w)) {
        return;
    }

    w->refWindow();

    Animation &animation = m_animations[w];

    animation.timeLine.reset();
    animation.parentY = 0;
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setDirection(TimeLine::Backward);
    animation.timeLine.setEasingCurve(QEasingCurve::Linear);

    const auto windows = effects->stackingOrder();
    auto parentIt = std::find_if(windows.constBegin(), windows.constEnd(),
        [w](EffectWindow *p) {
            return p->findModal() == w;
        });
    if (parentIt != windows.constEnd()) {
        animation.parentY = (*parentIt)->y();
    }

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    w->addRepaintFull();
}

void SheetEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

bool SheetEffect::isSheetWindow(EffectWindow *w) const
{
    return w->isModal();
}

} // namespace KWin
