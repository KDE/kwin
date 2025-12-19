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

template<typename T>
static T lerp(T a, T b, qreal progress)
{
    return a * (1 - progress) + b * progress;
}

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

SlideNotificationAnimation::SlideNotificationAnimation(EffectWindow *window)
    : window(window)
{
}

void SlideNotificationAnimation::revert()
{
    timeline.toggleDirection();
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

void SlideNotificationAnimation::apply(WindowPaintData &data)
{
    data += lerp(initialOffset, finalOffset, timeline.progress());
    data.multiplyOpacity(lerp(initialOpacity, finalOpacity, timeline.progress()));
}

DisplaceNotificationAnimation::DisplaceNotificationAnimation(EffectWindow *window)
    : window(window)
{
}

RectF DisplaceNotificationAnimation::dirtyArea() const
{
    const RectF visibleRect = window->expandedGeometry().translated(-window->pos());
    RectF dirtyArea;
    for (const QPointF &point : path) {
        dirtyArea |= visibleRect.translated(point);
    }
    return dirtyArea;
}

void DisplaceNotificationAnimation::moveTo(const QPointF &point)
{
    path.append(point);
    timeline.setDuration(timeline.duration() + Effect::animationTime(150ms));
}

void DisplaceNotificationAnimation::apply(WindowPaintData &data)
{
    const qreal totalProgress = timeline.progress();
    const int hopCount = path.size() - 1;
    const qreal progressPerHop = 1.0 / hopCount;
    int hop = std::floor(totalProgress / progressPerHop);
    if (hop >= hopCount) {
        hop = hopCount - 1;
    }
    const qreal hopProgress = (totalProgress - hop * progressPerHop) / progressPerHop;

    const QPointF currentPosition = lerp(path[hop], path[hop + 1], hopProgress);
    data += currentPosition - window->pos();
}

std::unique_ptr<DisplaceNotificationAnimation> DisplaceNotificationAnimation::move(EffectWindow *window, const QPointF &initialPosition, const QPointF &finalPosition)
{
    auto animation = std::make_unique<DisplaceNotificationAnimation>(window);
    animation->path.append(initialPosition);
    animation->path.append(finalPosition);
    animation->timeline.setDuration(Effect::animationTime(150ms));
    animation->timeline.setEasingCurve(QEasingCurve::Linear);
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
    return (!slide || slide->timeline.done())
        && (!displace || displace->timeline.done());
}

void NotificationAnimation::advance(RenderView *view)
{
    if (slide) {
        slide->timeline.advance(view);
    }
    if (displace) {
        displace->timeline.advance(view);
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

void SlidingNotificationsEffect::prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data)
{
    if (auto it = m_animations.find(window); it != m_animations.end()) {
        auto &[window, animation] = *it;

        data.setTransformed();
        animation->advance(view);
    }

    effects->prePaintWindow(view, window, data);
}

void SlidingNotificationsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const Region &deviceGeometry, WindowPaintData &data)
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

    effects->paintWindow(renderTarget, viewport, window, mask, clipped, data);
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
