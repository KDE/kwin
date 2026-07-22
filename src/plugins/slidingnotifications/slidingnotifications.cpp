/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/slidingnotifications/slidingnotifications.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "scene/windowitem.h"
#include "wayland/surface.h"
#include "window.h"

using namespace std::chrono_literals;

namespace KWin
{

static RectF panelGeometry(const EffectWindow *window)
{
    // The layer-shell protocol has no concept of window geometry, but for the animation
    // purposes, we want to know the actual bounds of the panel. Use the input shape instead.
    if (window->surface()) {
        return window->surface()->input().boundingRect().translated(window->window()->mapFromLocal(QPointF(0, 0)));
    } else {
        return window->frameGeometry();
    }
}

static qreal minOr(std::optional<qreal> a, qreal b)
{
    if (a) {
        return std::min(*a, b);
    } else {
        return b;
    }
}

struct NotificationObstacle
{
};

struct PanelObstacle
{
    qreal distance;
};

using Obstacle = std::variant<NotificationObstacle, PanelObstacle>;

static std::optional<PanelObstacle> obstacleToLeft(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();
    const qreal deadzoneMinLeft = notificationRect.left() - notificationRect.width();
    const RectF deadzoneRect = RectF(QPointF(std::max(screenRect.left(), deadzoneMinLeft), notificationRect.top()),
                                     QPointF(notificationRect.left(), notificationRect.bottom()));
    if (deadzoneRect.isEmpty()) {
        return std::nullopt;
    }

    std::optional<qreal> panelDistance = std::nullopt;

    for (const auto stack = effects->stackingOrder(); const EffectWindow *window : stack) {
        if (window->isDock() && !window->isHidden()) {
            const RectF panelRect = panelGeometry(window);
            if (panelRect.intersects(deadzoneRect)) {
                panelDistance = minOr(panelDistance, notificationRect.left() - panelRect.right());
            }
        }
    }

    return panelDistance.transform([](qreal distance) {
        return PanelObstacle{
            .distance = distance,
        };
    });
}

static std::optional<PanelObstacle> obstacleToRight(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();
    const qreal deadzoneMaxRight = notificationRect.right() + notificationRect.width();
    const RectF deadzoneRect = RectF(QPointF(notificationRect.right(), notificationRect.top()),
                                     QPointF(std::min(screenRect.right(), deadzoneMaxRight), notificationRect.bottom()));
    if (deadzoneRect.isEmpty()) {
        return std::nullopt;
    }

    std::optional<qreal> panelDistance = std::nullopt;

    for (const auto stack = effects->stackingOrder(); const EffectWindow *window : stack) {
        if (window->isDock() && !window->isHidden()) {
            const RectF panelRect = panelGeometry(window);
            if (panelRect.intersects(deadzoneRect)) {
                panelDistance = minOr(panelDistance, panelRect.left() - notificationRect.right());
            }
        }
    }

    return panelDistance.transform([](qreal distance) {
        return PanelObstacle{
            .distance = distance,
        };
    });
}

static std::optional<Obstacle> obstacleAbove(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();
    const qreal deadzoneMinTop = notificationRect.top() - notificationRect.height();
    const RectF deadzoneRect = RectF(QPointF(notificationRect.left(), std::max(screenRect.top(), deadzoneMinTop)),
                                     QPointF(notificationRect.right(), notificationRect.top()));
    if (deadzoneRect.isEmpty()) {
        return std::nullopt;
    }

    std::optional<qreal> panelDistance = std::nullopt;

    for (const auto stack = effects->stackingOrder(); const EffectWindow *window : stack) {
        if (window->isHidden()) {
            continue;
        }
        if (window->isNotification() || window->isCriticalNotification()) {
            if (window->frameGeometry().intersects(deadzoneRect)) {
                return NotificationObstacle{};
            }
        } else if (window->isDock()) {
            const RectF panelRect = panelGeometry(window);
            if (panelRect.intersects(deadzoneRect)) {
                panelDistance = minOr(panelDistance, notificationRect.top() - panelRect.bottom());
            }
        }
    }

    return panelDistance.transform([](qreal distance) {
        return PanelObstacle{
            .distance = distance,
        };
    });
}

static std::optional<Obstacle> obstacleBelow(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();
    const qreal deadzoneMaxBottom = notificationRect.bottom() + notificationRect.height();
    const RectF deadzoneRect = RectF(QPointF(notificationRect.left(), notificationRect.bottom()),
                                     QPointF(notificationRect.right(), std::min(screenRect.bottom(), deadzoneMaxBottom)));
    if (deadzoneRect.isEmpty()) {
        return std::nullopt;
    }

    std::optional<qreal> panelDistance = std::nullopt;

    for (const auto stack = effects->stackingOrder(); const EffectWindow *window : stack) {
        if (window->isHidden()) {
            continue;
        }
        if (window->isNotification() || window->isCriticalNotification()) {
            if (window->frameGeometry().intersects(deadzoneRect)) {
                return NotificationObstacle{};
            }
        } else if (window->isDock()) {
            const RectF panelRect = panelGeometry(window);
            if (panelRect.intersects(deadzoneRect)) {
                panelDistance = minOr(panelDistance, panelRect.top() - notificationRect.bottom());
            }
        }
    }

    return panelDistance.transform([](qreal distance) {
        return PanelObstacle{
            .distance = distance,
        };
    });
}

static QPointF slideOffset(const EffectWindow *notification)
{
    const RectF screenRect = notification->screen()->geometryF();
    const RectF notificationRect = notification->frameGeometry();

    if (notificationRect.left() < screenRect.horizontalCenter() && screenRect.horizontalCenter() < notificationRect.right()) {
        if (notificationRect.verticalCenter() < screenRect.verticalCenter()) {
            if (const auto obstacle = obstacleAbove(notification)) {
                if (const auto panelObstacle = std::get_if<PanelObstacle>(&*obstacle)) {
                    return -QPointF(0, panelObstacle->distance);
                } else {
                    return -QPointF(0, 0);
                }
            } else {
                const qreal distanceFromScreen = notificationRect.top() - screenRect.top();
                return -QPointF(0, distanceFromScreen < notificationRect.height() ? notificationRect.height() : 0);
            }
        } else {
            if (const auto obstacle = obstacleBelow(notification)) {
                if (const auto panelObstacle = std::get_if<PanelObstacle>(&*obstacle)) {
                    return QPointF(0, panelObstacle->distance);
                } else {
                    return QPointF(0, 0);
                }
            } else {
                const qreal distanceFromScreen = screenRect.bottom() - notificationRect.bottom();
                return QPointF(0, distanceFromScreen < notificationRect.height() ? notificationRect.height() : 0);
            }
        }
    }

    if (notificationRect.horizontalCenter() < screenRect.horizontalCenter()) {
        if (const auto panelObstacle = obstacleToLeft(notification)) {
            return -QPointF(panelObstacle->distance, 0);
        } else {
            const qreal distanceFromScreen = notificationRect.left() - screenRect.left();
            return -QPointF(distanceFromScreen < 0.5 * notificationRect.width() ? notificationRect.width() : 0.5 * notificationRect.width(), 0);
        }
    } else {
        if (const auto panelObstacle = obstacleToRight(notification)) {
            return QPointF(panelObstacle->distance, 0);
        } else {
            const qreal distanceFromScreen = screenRect.right() - notificationRect.right();
            return QPointF(distanceFromScreen < 0.5 * notificationRect.width() ? notificationRect.width() : 0.5 * notificationRect.width(), 0);
        }
    }
}

static SpringMotion makeSpring()
{
    return SpringMotion(900.0 / effects->animationTimeFactor(), 1.0);
}

static SpringMotion makeFadeSpring()
{
    // Stiffer than the motion spring, so the fade settles just before the slide does.
    SpringMotion spring(2000.0 / effects->animationTimeFactor(), 1.0);
    spring.setEpsilon(0.01); // opacity spans 0..1, so it needs a much finer stop threshold
    return spring;
}

SlideNotificationAnimation::SlideNotificationAnimation(EffectWindow *window)
    : window(window)
{
}

void SlideNotificationAnimation::revert()
{
    fade.setAnchor(0.0); // fade back out
    // Slide the notification back out the way it came in.
    springX.setAnchor(slideTarget.x());
    springY.setAnchor(slideTarget.y());
}

void SlideNotificationAnimation::advance(RenderView *view)
{
    const std::chrono::milliseconds delta = clock.tick(view);
    springX.advance(delta);
    springY.advance(delta);
    fade.advance(delta);
}

bool SlideNotificationAnimation::done() const
{
    return !fade.isMoving() && !springX.isMoving() && !springY.isMoving();
}

QPointF SlideNotificationAnimation::offset() const
{
    return QPointF(springX.position(), springY.position());
}

qreal SlideNotificationAnimation::opacity() const
{
    return fade.position();
}

std::unique_ptr<SlideNotificationAnimation> SlideNotificationAnimation::intro(EffectWindow *window)
{
    auto animation = std::make_unique<SlideNotificationAnimation>(window);
    const QPointF entry = slideOffset(window);
    animation->slideTarget = entry;
    animation->springX = makeSpring();
    animation->springY = makeSpring();
    animation->springX.setPosition(entry.x()); // spring in from the slid-away offset to 0
    animation->springY.setPosition(entry.y());
    animation->fade = makeFadeSpring();
    animation->fade.setPosition(0.0); // fade in from transparent to opaque
    animation->fade.setAnchor(1.0);
    return animation;
}

std::unique_ptr<SlideNotificationAnimation> SlideNotificationAnimation::outro(EffectWindow *window)
{
    auto animation = std::make_unique<SlideNotificationAnimation>(window);
    const QPointF exit = slideOffset(window);
    animation->slideTarget = exit;
    animation->springX = makeSpring();
    animation->springY = makeSpring();
    animation->springX.setAnchor(exit.x()); // slide away from 0 to the slid-away offset
    animation->springY.setAnchor(exit.y());
    animation->fade = makeFadeSpring();
    animation->fade.setPosition(1.0); // fade out from opaque to transparent
    animation->fade.setAnchor(0.0);
    return animation;
}

RectF SlideNotificationAnimation::clipArea() const
{
    return window->screen()->geometryF();
}

RectF SlideNotificationAnimation::dirtyArea() const
{
    const RectF current = window->expandedGeometry().translated(offset());
    const RectF target = window->expandedGeometry().translated(QPointF(springX.anchor(), springY.anchor()));
    return current.united(target).intersected(clipArea());
}

void SlideNotificationAnimation::apply(WindowPaintData &data)
{
    data += offset();
    data.multiplyOpacity(opacity());
}

DisplaceNotificationAnimation::DisplaceNotificationAnimation(EffectWindow *window)
    : window(window)
{
}

QPointF DisplaceNotificationAnimation::position() const
{
    return QPointF(springX.position(), springY.position());
}

RectF DisplaceNotificationAnimation::dirtyArea() const
{
    const RectF visibleRect = window->expandedGeometry().translated(-window->pos());
    const RectF current = visibleRect.translated(position());
    const RectF target = visibleRect.translated(QPointF(springX.anchor(), springY.anchor()));
    return current.united(target);
}

void DisplaceNotificationAnimation::moveTo(const QPointF &point)
{
    springX.setAnchor(point.x());
    springY.setAnchor(point.y());
}

void DisplaceNotificationAnimation::advance(RenderView *view)
{
    const std::chrono::milliseconds delta = clock.tick(view);
    springX.advance(delta);
    springY.advance(delta);
}

bool DisplaceNotificationAnimation::done() const
{
    return !springX.isMoving() && !springY.isMoving();
}

void DisplaceNotificationAnimation::apply(WindowPaintData &data)
{
    data += position() - window->pos();
}

std::unique_ptr<DisplaceNotificationAnimation> DisplaceNotificationAnimation::move(EffectWindow *window, const QPointF &initialPosition, const QPointF &finalPosition)
{
    auto animation = std::make_unique<DisplaceNotificationAnimation>(window);
    animation->springX = makeSpring();
    animation->springY = makeSpring();
    animation->springX.setPosition(initialPosition.x());
    animation->springY.setPosition(initialPosition.y());
    animation->springX.setAnchor(finalPosition.x());
    animation->springY.setAnchor(finalPosition.y());
    return animation;
}

NotificationAnimation::NotificationAnimation(EffectWindow *window)
    : window(window)
    , effect(window->windowItem())
    , deletedRef(window)
{
    window->setData(WindowForceBlurRole, true);
}

NotificationAnimation::~NotificationAnimation()
{
    window->setData(WindowForceBlurRole, QVariant());
}

bool NotificationAnimation::isFinished() const
{
    return (!slide || slide->done())
        && (!displace || displace->done());
}

void NotificationAnimation::advance(RenderView *view)
{
    if (slide) {
        slide->advance(view);
    }
    if (displace) {
        displace->advance(view);
    }
}

RectF NotificationAnimation::dirtyArea() const
{
    RectF dirtyArea;
    if (slide) {
        dirtyArea |= slide->dirtyArea();
    }
    if (displace) {
        dirtyArea |= displace->dirtyArea();
    }
    return dirtyArea;
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

bool SlidingNotificationsEffect::blocksDirectScanout() const
{
    return false;
}

void SlidingNotificationsEffect::prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data)
{
    if (auto it = m_animations.find(window); it != m_animations.end()) {
        auto &[window, animation] = *it;

        data.setTransformed();
        animation->advance(view);
    }

    effects->prePaintWindow(view, window, data);
}

bool SlidingNotificationsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const Region &deviceGeometry, WindowPaintData &data)
{
    const auto it = m_animations.find(window);
    if (it == m_animations.end()) {
        return effects->paintWindow(renderTarget, viewport, window, mask, deviceGeometry, data);
    }

    const auto &[_, animation] = *it;
    Region clipped = deviceGeometry;

    if (animation->slide) {
        clipped &= viewport.mapToDeviceCoordinates(animation->slide->clipArea()).rounded();
        animation->slide->apply(data);
    }

    if (animation->displace) {
        animation->displace->apply(data);
    }

    return effects->paintWindow(renderTarget, viewport, window, mask, clipped, data);
}

void SlidingNotificationsEffect::postPaintScreen()
{
    std::erase_if(m_animations, [](const auto &it) {
        const auto &[window, animation] = it;
        return animation->isFinished();
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
    if (!window->isNotification() && !window->isCriticalNotification()) {
        return;
    }

    connect(window, &EffectWindow::windowFrameGeometryChanged, this, &SlidingNotificationsEffect::onWindowFrameGeometryChanged);

    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    window->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    auto &animation = m_animations[window];
    if (!animation) {
        animation = std::make_unique<NotificationAnimation>(window);
    }

    animation->slide = SlideNotificationAnimation::intro(window);
    window->addLayerRepaint(animation->dirtyArea());
}

void SlidingNotificationsEffect::onWindowClosed(EffectWindow *window)
{
    if (!window->isNotification() && !window->isCriticalNotification()) {
        return;
    }

    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    window->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    auto &animation = m_animations[window];
    if (!animation) {
        animation = std::make_unique<NotificationAnimation>(window);
    }

    if (animation->slide) {
        // If the notification already has an animation, it's the intro animation.
        animation->slide->revert();
    } else {
        animation->slide = SlideNotificationAnimation::outro(window);
    }

    window->addLayerRepaint(animation->dirtyArea());
}

void SlidingNotificationsEffect::onWindowFrameGeometryChanged(EffectWindow *window, const RectF &oldGeometry)
{
    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    if (window->pos() == oldGeometry.topLeft()) {
        return;
    }

    // If the notification both resizes and moves (e.g. to accommodate for the new size), skip the
    // displace animation unless the window is already animated (to avoid sudden jumps).
    if (window->size() != oldGeometry.size() && !m_animations.contains(window)) {
        return;
    }

    auto &animation = m_animations[window];
    if (!animation) {
        animation = std::make_unique<NotificationAnimation>(window);
    }

    if (animation->displace) {
        // The notification has been moved while we're already playing another reposition animation.
        // Move the window to the new position after the previous transition completes.
        animation->displace->moveTo(window->pos());
    } else {
        animation->displace = DisplaceNotificationAnimation::move(window, oldGeometry.topLeft(), window->pos());
    }

    window->addLayerRepaint(animation->dirtyArea());
}

}
