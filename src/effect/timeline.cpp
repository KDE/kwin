/*
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/timeline.h"
#include "scene/scene.h"

namespace KWin
{

AnimationClock::AnimationClock()
{
}

void AnimationClock::reset()
{
    m_lastTimestamp.reset();
}

std::chrono::milliseconds AnimationClock::tick(std::chrono::nanoseconds timestamp)
{
    std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
    if (m_lastTimestamp) {
        if (timestamp < m_lastTimestamp) {
            return std::chrono::milliseconds::zero();
        }
        delta = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - *m_lastTimestamp);
    }

    Q_ASSERT(delta >= std::chrono::milliseconds::zero());
    m_lastTimestamp = timestamp;

    return delta;
}

std::chrono::milliseconds AnimationClock::tick(const RenderView *view)
{
    return tick(view->nextPresentationTimestamp());
}

class Q_DECL_HIDDEN TimeLine::Data : public QSharedData
{
public:
    AnimationClock clock;
    std::chrono::milliseconds duration;
    Direction direction;
    QEasingCurve easingCurve;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds::zero();
    bool done = false;
    RedirectMode sourceRedirectMode = RedirectMode::Relaxed;
    RedirectMode targetRedirectMode = RedirectMode::Strict;
};

TimeLine::TimeLine(std::chrono::milliseconds duration, Direction direction)
    : d(new Data)
{
    Q_ASSERT(duration > std::chrono::milliseconds::zero());
    d->duration = duration;
    d->direction = direction;
}

TimeLine::TimeLine(const TimeLine &other)
    : d(other.d)
{
}

TimeLine::~TimeLine() = default;

qreal TimeLine::progress() const
{
    return static_cast<qreal>(d->elapsed.count()) / d->duration.count();
}

qreal TimeLine::value() const
{
    const qreal t = progress();
    return d->easingCurve.valueForProgress(
        d->direction == Backward ? 1.0 - t : t);
}

void TimeLine::advance(std::chrono::nanoseconds timestamp)
{
    if (d->done) {
        return;
    }

    d->elapsed += d->clock.tick(timestamp);
    if (d->elapsed >= d->duration) {
        d->elapsed = d->duration;
        d->done = true;
        d->clock.reset();
    }
}

void TimeLine::advance(const RenderView *view)
{
    advance(view->nextPresentationTimestamp());
}

std::chrono::milliseconds TimeLine::elapsed() const
{
    return d->elapsed;
}

void TimeLine::setElapsed(std::chrono::milliseconds elapsed)
{
    Q_ASSERT(elapsed >= std::chrono::milliseconds::zero());
    if (elapsed == d->elapsed) {
        return;
    }

    reset();

    d->elapsed = elapsed;

    if (d->elapsed >= d->duration) {
        d->elapsed = d->duration;
        d->done = true;
        d->clock.reset();
    }
}

std::chrono::milliseconds TimeLine::duration() const
{
    return d->duration;
}

void TimeLine::setDuration(std::chrono::milliseconds duration)
{
    Q_ASSERT(duration > std::chrono::milliseconds::zero());
    if (duration == d->duration) {
        return;
    }
    d->elapsed = std::chrono::milliseconds(qRound(progress() * duration.count()));
    d->duration = duration;
    if (d->elapsed == d->duration) {
        d->done = true;
        d->clock.reset();
    }
}

TimeLine::Direction TimeLine::direction() const
{
    return d->direction;
}

void TimeLine::setDirection(TimeLine::Direction direction)
{
    if (d->direction == direction) {
        return;
    }

    d->direction = direction;

    if (d->elapsed > std::chrono::milliseconds::zero()
        || d->sourceRedirectMode == RedirectMode::Strict) {
        d->elapsed = d->duration - d->elapsed;
    }

    if (d->done && d->targetRedirectMode == RedirectMode::Relaxed) {
        d->done = false;
    }

    if (d->elapsed >= d->duration) {
        d->done = true;
        d->clock.reset();
    }
}

void TimeLine::toggleDirection()
{
    setDirection(d->direction == Forward ? Backward : Forward);
}

QEasingCurve TimeLine::easingCurve() const
{
    return d->easingCurve;
}

void TimeLine::setEasingCurve(const QEasingCurve &easingCurve)
{
    d->easingCurve = easingCurve;
}

void TimeLine::setEasingCurve(QEasingCurve::Type type)
{
    d->easingCurve.setType(type);
}

bool TimeLine::running() const
{
    return d->elapsed != std::chrono::milliseconds::zero()
        && d->elapsed != d->duration;
}

bool TimeLine::done() const
{
    return d->done;
}

void TimeLine::reset()
{
    d->clock.reset();
    d->elapsed = std::chrono::milliseconds::zero();
    d->done = false;
}

TimeLine::RedirectMode TimeLine::sourceRedirectMode() const
{
    return d->sourceRedirectMode;
}

void TimeLine::setSourceRedirectMode(RedirectMode mode)
{
    d->sourceRedirectMode = mode;
}

TimeLine::RedirectMode TimeLine::targetRedirectMode() const
{
    return d->targetRedirectMode;
}

void TimeLine::setTargetRedirectMode(RedirectMode mode)
{
    d->targetRedirectMode = mode;
}

TimeLine &TimeLine::operator=(const TimeLine &other)
{
    d = other.d;
    return *this;
}

} // namespace KWin
