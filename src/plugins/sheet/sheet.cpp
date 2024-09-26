/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "sheet.h"

// KConfigSkeleton
#include "sheetconfig.h"

#include "core/renderviewport.h"
#include "effect/effecthandler.h"

// Qt
#include <QMatrix4x4>
#include <qmath.h>

using namespace std::chrono_literals;

namespace KWin
{

SheetEffect::SheetEffect()
{
    SheetConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &SheetEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SheetEffect::slotWindowClosed);
}

void SheetEffect::reconfigure(ReconfigureFlags flags)
{
    SheetConfig::self()->read();

    // TODO: Rename AnimationTime config key to Duration.
    const double d = animationTime(SheetConfig::animationTime() != 0
                                       ? std::chrono::milliseconds(SheetConfig::animationTime())
                                       : 300ms);
    m_duration = std::chrono::milliseconds(int(d));
}

void SheetEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto animationIt = m_animations.find(w);
    if (animationIt != m_animations.end()) {
        (*animationIt).timeLine.advance(presentTime);
        data.setTransformed();
    }

    effects->prePaintWindow(w, data, presentTime);
}

void SheetEffect::apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    auto animationIt = m_animations.constFind(window);
    if (animationIt == m_animations.constEnd()) {
        return;
    }

    const qreal t = (*animationIt).timeLine.value();

    const QRectF rect = window->expandedGeometry().translated(-window->pos());
    const float fovY = std::tan(qDegreesToRadians(60.0f) / 2);
    const float aspect = rect.width() / rect.height();
    const float zNear = 0.1f;
    const float zFar = 100.0f;

    const float yMax = zNear * fovY;
    const float yMin = -yMax;
    const float xMin = yMin * aspect;
    const float xMax = yMax * aspect;

    const float scaleFactor = 1.1 * fovY / yMax;

    QMatrix4x4 matrix;
    matrix.viewport(rect);
    matrix.frustum(xMin, xMax, yMax, yMin, zNear, zFar);
    matrix.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    matrix.scale((xMax - xMin) * scaleFactor / rect.width(), -(yMax - yMin) * scaleFactor / rect.height(), 0.001);
    matrix.translate(-rect.x(), -rect.y());

    matrix.scale(1.0, t, t);
    matrix.translate(0.0, -interpolate(window->y() - (*animationIt).parentY, 0.0, t));

    matrix.translate(window->width() / 2, 0);
    matrix.rotate(interpolate(60.0, 0.0, t), 1, 0, 0);
    matrix.translate(-window->width() / 2, 0);

    for (WindowQuad &quad : quads) {
        for (int i = 0; i < 4; ++i) {
            const QPointF transformed = matrix.map(QPointF(quad[i].x(), quad[i].y()));
            quad[i].setX(transformed.x());
            quad[i].setY(transformed.y());
        }
    }

    data.multiplyOpacity(t);
}

void SheetEffect::postPaintWindow(EffectWindow *w)
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        EffectWindow *w = animationIt.key();
        w->addRepaintFull();
        if ((*animationIt).timeLine.done()) {
            unredirect(w);
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
    animation.deletedRef = EffectWindowDeletedRef(w);
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

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    redirect(w);
    w->addRepaintFull();
}

void SheetEffect::slotWindowClosed(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isSheetWindow(w) || w->skipsCloseAnimation()) {
        return;
    }

    Animation &animation = m_animations[w];
    animation.deletedRef = EffectWindowDeletedRef(w);
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

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    redirect(w);
    w->addRepaintFull();
}

bool SheetEffect::isSheetWindow(EffectWindow *w) const
{
    return w->isModal();
}

} // namespace KWin

#include "moc_sheet.cpp"
