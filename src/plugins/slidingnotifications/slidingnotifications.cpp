/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/slidingnotifications/slidingnotifications.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"

using namespace std::chrono_literals;

namespace KWin
{

static bool isNotificationObstructedOnLeft(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();
    const RectF deadzoneRect = RectF(QPointF(screenRect.left(), notificationRect.top()),
                                     QPointF(notificationRect.left(), notificationRect.bottom()));
    if (deadzoneRect.isEmpty()) {
        return false;
    }

    const auto stack = effects->stackingOrder();
    for (const EffectWindow *window : stack) {
        if (window->isHidden()) {
            continue;
        }
        if (window->isDock() && window->frameGeometry().intersects(deadzoneRect)) {
            return true;
        }
    }

    return false;
}

static bool isNotificationObstructedOnRight(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();
    const RectF deadzoneRect = RectF(QPointF(notificationRect.right(), notificationRect.top()),
                                     QPointF(screenRect.right(), notificationRect.bottom()));
    if (deadzoneRect.isEmpty()) {
        return false;
    }

    const auto stack = effects->stackingOrder();
    for (const EffectWindow *window : stack) {
        if (window->isHidden()) {
            continue;
        }
        if (window->isDock() && window->frameGeometry().intersects(deadzoneRect)) {
            return true;
        }
    }

    return false;
}

static QPointF slideOffset(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();

    if (notificationRect.left() < screenRect.horizontalCenter() && screenRect.horizontalCenter() < notificationRect.right()) {
        return QPointF(0, 0);
    }

    if (notificationRect.horizontalCenter() < screenRect.horizontalCenter()) {
        if (isNotificationObstructedOnLeft(notification)) {
            return QPointF(0, 0);
        } else {
            const qreal slideDistance = notificationRect.left() - screenRect.left();
            return -QPointF(slideDistance < notificationRect.width() / 2 ? notificationRect.width() : 0.5 * notificationRect.width(), 0);
        }
    } else {
        if (isNotificationObstructedOnRight(notification)) {
            return QPointF(0, 0);
        } else {
            const qreal slideDistance = screenRect.right() - notificationRect.right();
            return QPointF(slideDistance < notificationRect.width() / 2 ? notificationRect.width() : 0.5 * notificationRect.width(), 0);
        }
    }
}

SlideNotificationAnimation::SlideNotificationAnimation(EffectWindow *window)
    : window(window)
    , deletedRef(window)
{
    window->setData(WindowForceBlurRole, true);
}

SlideNotificationAnimation::~SlideNotificationAnimation()
{
    window->setData(WindowForceBlurRole, QVariant());
}

static QEasingCurve cubicBezier(const QPointF &c0, const QPointF &c1)
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(c0, c1, QPointF(1, 1));
    return curve;
}

std::unique_ptr<SlideNotificationAnimation> SlideNotificationAnimation::intro(EffectWindow *window)
{
    auto animation = std::make_unique<SlideNotificationAnimation>(window);
    animation->timeline.setDuration(Effect::animationTime(200ms));
    animation->timeline.setEasingCurve(cubicBezier(QPointF(0, 1), QPointF(0, 1)));
    animation->initialOffset = slideOffset(window);
    animation->finalOffset = QPointF(0, 0);
    animation->initialOpacity = 0.0;
    animation->finalOpacity = 1.0;
    return animation;
}

std::unique_ptr<SlideNotificationAnimation> SlideNotificationAnimation::outro(EffectWindow *window)
{
    auto animation = std::make_unique<SlideNotificationAnimation>(window);
    animation->timeline.setDuration(Effect::animationTime(200ms));
    animation->timeline.setEasingCurve(cubicBezier(QPointF(1, 0), QPointF(1, 0)));
    animation->initialOffset = QPointF(0, 0);
    animation->finalOffset = slideOffset(window);
    animation->initialOpacity = 1.0;
    animation->finalOpacity = 0.0;
    return animation;
}

RectF SlideNotificationAnimation::clipArea() const
{
    return window->screen()->geometryF();
}

RectF SlideNotificationAnimation::dirtyArea() const
{
    const RectF start = window->expandedGeometry().translated(initialOffset);
    const RectF end = window->expandedGeometry().translated(finalOffset);
    return start.united(end).intersected(clipArea());
}

SlidingNotificationsEffect::SlidingNotificationsEffect()
{
    connect(effects, &EffectsHandler::windowAdded, this, &SlidingNotificationsEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SlidingNotificationsEffect::onWindowClosed);
}

bool SlidingNotificationsEffect::isActive() const
{
    return !m_animations.empty();
}

int SlidingNotificationsEffect::requestedEffectChainPosition() const
{
    return 40;
}

void SlidingNotificationsEffect::prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data)
{
    const auto &[first, last] = m_animations.equal_range(window);
    for (auto it = first; it != last; ++it) {
        auto &[window, animation] = *it;

        data.setTransformed();
        animation->timeline.advance(view);
    }

    effects->prePaintWindow(view, window, data);
}

template<typename T>
static T lerp(T a, T b, qreal progress)
{
    return a * (1 - progress) + b * progress;
}

void SlidingNotificationsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const Region &deviceGeometry, WindowPaintData &data)
{
    const auto &[first, last] = m_animations.equal_range(window);
    if (first == last) {
        return effects->paintWindow(renderTarget, viewport, window, mask, deviceGeometry, data);
    }

    Region clipped = deviceGeometry;
    for (auto it = first; it != last; ++it) {
        const auto &[_, animation] = *it;

        clipped &= viewport.mapToDeviceCoordinates(animation->clipArea()).rounded();
        data += lerp(animation->initialOffset, animation->finalOffset, animation->timeline.progress());
        data.multiplyOpacity(lerp(animation->initialOpacity, animation->finalOpacity, animation->timeline.progress()));
    }

    effects->paintWindow(renderTarget, viewport, window, mask, clipped, data);
}

void SlidingNotificationsEffect::postPaintScreen()
{
    std::erase_if(m_animations, [](const auto &it) {
        const auto &[window, animation] = it;
        return animation->timeline.done();
    });

    for (const auto &[window, animation] : m_animations) {
        window->addLayerRepaint(animation->dirtyArea());
    }

    effects->postPaintScreen();
}

bool SlidingNotificationsEffect::supported()
{
    return effects->animationsSupported();
}

void SlidingNotificationsEffect::onWindowAdded(EffectWindow *window)
{
    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    if (!window->isNotification() && !window->isCriticalNotification()) {
        return;
    }

    window->setData(WindowAddedGrabRole, QVariant::fromValue(this));

    auto &[_, animation] = *m_animations.insert({window, SlideNotificationAnimation::intro(window)});
    window->addLayerRepaint(animation->dirtyArea());
}

void SlidingNotificationsEffect::onWindowClosed(EffectWindow *window)
{
    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    if (!window->isNotification() && !window->isCriticalNotification()) {
        return;
    }

    window->setData(WindowClosedGrabRole, QVariant::fromValue(this));

    auto &[_, animation] = *m_animations.insert({window, SlideNotificationAnimation::outro(window)});
    window->addLayerRepaint(animation->dirtyArea());
}

} // namespace KWin
