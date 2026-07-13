/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/slideback/motionmanager.h"
#include "scene/transformitem.h"
#include "scene/windowitem.h"

namespace KWin
{

/***************************************************************
 Motion1D
***************************************************************/

Motion1D::Motion1D(double initial, double strength, double smoothness)
    : Motion<double>(initial, strength, smoothness)
{
}

Motion1D::Motion1D(const Motion1D &other)
    : Motion<double>(other)
{
}

Motion1D::~Motion1D()
{
}

/***************************************************************
 Motion2D
***************************************************************/

Motion2D::Motion2D(QPointF initial, double strength, double smoothness)
    : Motion<QPointF>(initial, strength, smoothness)
{
}

Motion2D::Motion2D(const Motion2D &other)
    : Motion<QPointF>(other)
{
}

Motion2D::~Motion2D()
{
}

/***************************************************************
 WindowMotionManager
***************************************************************/

WindowMotionManager::WindowMotionManager(bool useGlobalAnimationModifier)
    : m_useGlobalAnimationModifier(useGlobalAnimationModifier)

{
    // TODO: Allow developer to modify motion attributes
} // TODO: What happens when the window moves by an external force?

WindowMotionManager::~WindowMotionManager()
{
}

void WindowMotionManager::manage(EffectWindow *w)
{
    if (m_managedWindows.contains(w)) {
        return;
    }

    double strength = 0.12;
    double smoothness = 2.5;
    if (m_useGlobalAnimationModifier && effects->animationTimeFactor()) {
        // If the factor is == 0 then we just skip the calculation completely
        strength = 0.12 / effects->animationTimeFactor();
        smoothness = effects->animationTimeFactor() * 2.5;
    }

    WindowMotion &motion = m_managedWindows[w];
    motion.translation.setStrength(strength);
    motion.translation.setSmoothness(smoothness);
    motion.scale.setStrength(strength * 1.33);
    motion.scale.setSmoothness(smoothness / 2.0);

    motion.translation.setValue(w->pos());
    motion.scale.setValue(QPointF(1.0, 1.0));

    motion.item = std::make_unique<TransformItem>(w);
}

void WindowMotionManager::unmanage(EffectWindow *w)
{
    m_movingWindowsSet.remove(w);
    m_managedWindows.erase(w);
}

void WindowMotionManager::unmanageAll()
{
    m_managedWindows.clear();
    m_movingWindowsSet.clear();
}

void WindowMotionManager::calculate(int time)
{
    if (!effects->animationTimeFactor()) {
        // Just skip it completely if the user wants no animation
        m_movingWindowsSet.clear();
        for (auto &[window, motion] : m_managedWindows) {
            motion.translation.finish();
            motion.scale.finish();
        }
    }

    for (auto &[window, motion] : m_managedWindows) {
        int stopped = 0;

        // TODO: What happens when distance() == 0 but we are still moving fast?
        // TODO: Motion needs to be calculated from the window's center

        Motion2D &trans = motion.translation;
        if (trans.distance().isNull()) {
            ++stopped;
        } else {
            // Still moving
            trans.calculate(time);
            const short fx = trans.target().x() <= trans.startValue().x() ? -1 : 1;
            const short fy = trans.target().y() <= trans.startValue().y() ? -1 : 1;
            if (trans.distance().x() * fx / 0.5 < 1.0 && trans.velocity().x() * fx / 0.2 < 1.0
                && trans.distance().y() * fy / 0.5 < 1.0 && trans.velocity().y() * fy / 0.2 < 1.0) {
                // Hide tiny oscillations
                motion.translation.finish();
                ++stopped;
            }
        }

        Motion2D &scale = motion.scale;
        if (scale.distance().isNull()) {
            ++stopped;
        } else {
            // Still scaling
            scale.calculate(time);
            const short fx = scale.target().x() < 1.0 ? -1 : 1;
            const short fy = scale.target().y() < 1.0 ? -1 : 1;
            if (scale.distance().x() * fx / 0.001 < 1.0 && scale.velocity().x() * fx / 0.05 < 1.0
                && scale.distance().y() * fy / 0.001 < 1.0 && scale.velocity().y() * fy / 0.05 < 1.0) {
                // Hide tiny oscillations
                motion.scale.finish();
                ++stopped;
            }
        }

        // We just finished this window's motion
        if (stopped == 2) {
            m_movingWindowsSet.remove(window);
        } else {
            QTransform transform;
            transform.scale(motion.scale.value().x(), motion.scale.value().y());
            motion.item->setPosition((motion.translation.value() - QPointF(window->x(), window->y())));
            motion.item->setTransform(transform);
        }
    }
}

void WindowMotionManager::reset()
{
    for (auto &[window, motion] : m_managedWindows) {
        motion.translation.setTarget(window->pos());
        motion.translation.finish();
        motion.scale.setTarget(QPointF(1.0, 1.0));
        motion.scale.finish();
    }
}

void WindowMotionManager::reset(EffectWindow *w)
{
    const auto it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }

    auto &[window, motion] = *it;
    motion.translation.setTarget(w->pos());
    motion.translation.finish();
    motion.scale.setTarget(QPointF(1.0, 1.0));
    motion.scale.finish();
}

void WindowMotionManager::moveWindow(EffectWindow *w, QPoint target, double scale, double yScale)
{
    Q_ASSERT(m_managedWindows.contains(w));
    auto &motion = m_managedWindows[w];

    if (yScale == 0.0) {
        yScale = scale;
    }
    QPointF scalePoint(scale, yScale);

    if (motion.translation.value() == target && motion.scale.value() == scalePoint) {
        return; // Window already at that position
    }

    motion.translation.setTarget(target);
    motion.scale.setTarget(scalePoint);

    m_movingWindowsSet << w;
}

QRectF WindowMotionManager::transformedGeometry(EffectWindow *w) const
{
    const auto it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return w->frameGeometry();
    }

    auto &[window, motion] = *it;

    QRectF geometry(w->frameGeometry());
    geometry.moveTo(motion.translation.value());
    geometry.setWidth(geometry.width() * motion.scale.value().x());
    geometry.setHeight(geometry.height() * motion.scale.value().y());

    return geometry;
}

void WindowMotionManager::setTransformedGeometry(EffectWindow *w, const QRectF &geometry)
{
    const auto it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }
    auto &[window, motion] = *it;
    motion.translation.setValue(geometry.topLeft());
    motion.scale.setValue(QPointF(geometry.width() / qreal(w->width()), geometry.height() / qreal(w->height())));
}

QRectF WindowMotionManager::targetGeometry(EffectWindow *w) const
{
    const auto it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return w->frameGeometry();
    }

    auto &[window, motion] = *it;
    QRectF geometry(w->frameGeometry());

    geometry.moveTo(motion.translation.target());
    geometry.setWidth(geometry.width() * motion.scale.target().x());
    geometry.setHeight(geometry.height() * motion.scale.target().y());

    return geometry;
}

EffectWindow *WindowMotionManager::windowAtPoint(QPoint point, bool useStackingOrder) const
{
    // TODO: Stacking order uses EffectsHandler::stackingOrder() then filters by m_managedWindows
    const auto it = std::ranges::find_if(m_managedWindows, [this, point](const auto &pair) {
        const auto &[window, motion] = pair;
        return RectF(transformedGeometry(window)).contains(point);
    });
    return it == m_managedWindows.end() ? nullptr : it->first;
}

} // namespace KWin
