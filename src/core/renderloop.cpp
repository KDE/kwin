/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderloop.h"
#include "renderloop_p.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_wayland.h"
#include "utils/common.h"
#include "wayland/surface_interface.h"

namespace KWin
{

template<typename T>
T alignTimestamp(const T &timestamp, const T &alignment)
{
    return timestamp + ((alignment - (timestamp % alignment)) % alignment);
}

RenderLoopPrivate *RenderLoopPrivate::get(RenderLoop *loop)
{
    return loop->d.get();
}

RenderLoopPrivate::RenderLoopPrivate(RenderLoop *q)
    : q(q)
{
    compositeTimer.setSingleShot(true);
    QObject::connect(&compositeTimer, &QTimer::timeout, q, [this]() {
        dispatch();
    });
}

void RenderLoopPrivate::scheduleRepaint()
{
    if (kwinApp()->isTerminating() || (compositeTimer.isActive() && !allowTearing)) {
        return;
    }
    if (vrrPolicy == RenderLoop::VrrPolicy::Always || (vrrPolicy == RenderLoop::VrrPolicy::Automatic && fullscreenItem != nullptr)) {
        presentMode = allowTearing ? SyncMode::AdaptiveAsync : SyncMode::Adaptive;
    } else {
        presentMode = allowTearing ? SyncMode::Async : SyncMode::Fixed;
    }
    const std::chrono::nanoseconds vblankInterval(1'000'000'000'000ull / refreshRate);
    const std::chrono::nanoseconds currentTime(std::chrono::steady_clock::now().time_since_epoch());

    // Estimate when the next presentation will occur. Note that this is a prediction.
    nextPresentationTimestamp = lastPresentationTimestamp + vblankInterval;
    if (nextPresentationTimestamp < currentTime && presentMode == SyncMode::Fixed) {
        nextPresentationTimestamp = lastPresentationTimestamp
            + alignTimestamp(currentTime - lastPresentationTimestamp, vblankInterval);
    }

    // Estimate when it's a good time to perform the next compositing cycle.
    const std::chrono::nanoseconds safetyMargin = std::chrono::milliseconds(3);

    std::chrono::nanoseconds renderTime;
    switch (q->latencyPolicy()) {
    case LatencyExtremelyLow:
        renderTime = std::chrono::nanoseconds(long(vblankInterval.count() * 0.1));
        break;
    case LatencyLow:
        renderTime = std::chrono::nanoseconds(long(vblankInterval.count() * 0.25));
        break;
    case LatencyMedium:
        renderTime = std::chrono::nanoseconds(long(vblankInterval.count() * 0.5));
        break;
    case LatencyHigh:
        renderTime = std::chrono::nanoseconds(long(vblankInterval.count() * 0.75));
        break;
    case LatencyExtremelyHigh:
        renderTime = std::chrono::nanoseconds(long(vblankInterval.count() * 0.9));
        break;
    }

    switch (options->renderTimeEstimator()) {
    case RenderTimeEstimatorMinimum:
        renderTime = std::max(renderTime, renderJournal.minimum());
        break;
    case RenderTimeEstimatorMaximum:
        renderTime = std::max(renderTime, renderJournal.maximum());
        break;
    case RenderTimeEstimatorAverage:
        renderTime = std::max(renderTime, renderJournal.average());
        break;
    }

    std::chrono::nanoseconds nextRenderTimestamp = nextPresentationTimestamp - renderTime - safetyMargin;

    // If we can't render the frame before the deadline, start compositing immediately.
    if (nextRenderTimestamp < currentTime) {
        nextRenderTimestamp = currentTime;
    }

    if (presentMode == SyncMode::Async || presentMode == SyncMode::AdaptiveAsync) {
        compositeTimer.start(0);
    } else {
        const std::chrono::nanoseconds waitInterval = nextRenderTimestamp - currentTime;
        compositeTimer.start(std::chrono::duration_cast<std::chrono::milliseconds>(waitInterval));
    }
}

void RenderLoopPrivate::delayScheduleRepaint()
{
    pendingReschedule = true;
}

void RenderLoopPrivate::maybeScheduleRepaint()
{
    if (pendingReschedule) {
        scheduleRepaint();
        pendingReschedule = false;
    }
}

void RenderLoopPrivate::notifyFrameFailed()
{
    Q_ASSERT(pendingFrameCount > 0);
    pendingFrameCount--;

    if (!inhibitCount) {
        maybeScheduleRepaint();
    }
}

void RenderLoopPrivate::notifyFrameCompleted(std::chrono::nanoseconds timestamp)
{
    Q_ASSERT(pendingFrameCount > 0);
    pendingFrameCount--;

    if (lastPresentationTimestamp <= timestamp) {
        lastPresentationTimestamp = timestamp;
    } else {
        qCDebug(KWIN_CORE,
                "Got invalid presentation timestamp: %lld (current %lld)",
                static_cast<long long>(timestamp.count()),
                static_cast<long long>(lastPresentationTimestamp.count()));
        lastPresentationTimestamp = std::chrono::steady_clock::now().time_since_epoch();
    }

    if (!inhibitCount) {
        maybeScheduleRepaint();
    }

    Q_EMIT q->framePresented(q, timestamp);
}

void RenderLoopPrivate::dispatch()
{
    // On X11, we want to ignore repaints that are scheduled by windows right before
    // the Compositor starts repainting.
    pendingRepaint = true;

    Q_EMIT q->frameRequested(q);

    // The Compositor may decide to not repaint when the frameRequested() signal is
    // emitted, in which case the pending repaint flag has to be reset manually.
    pendingRepaint = false;
}

void RenderLoopPrivate::invalidate()
{
    pendingReschedule = false;
    pendingFrameCount = 0;
    compositeTimer.stop();
}

RenderLoop::RenderLoop()
    : d(std::make_unique<RenderLoopPrivate>(this))
{
}

RenderLoop::~RenderLoop()
{
}

void RenderLoop::inhibit()
{
    d->inhibitCount++;

    if (d->inhibitCount == 1) {
        d->compositeTimer.stop();
    }
}

void RenderLoop::uninhibit()
{
    Q_ASSERT(d->inhibitCount > 0);
    d->inhibitCount--;

    if (d->inhibitCount == 0) {
        d->maybeScheduleRepaint();
    }
}

void RenderLoop::beginFrame()
{
    d->pendingRepaint = false;
    d->pendingFrameCount++;
    d->renderJournal.beginFrame();
}

void RenderLoop::endFrame()
{
    d->renderJournal.endFrame();
}

int RenderLoop::refreshRate() const
{
    return d->refreshRate;
}

void RenderLoop::setRefreshRate(int refreshRate)
{
    if (d->refreshRate == refreshRate) {
        return;
    }
    d->refreshRate = refreshRate;
    Q_EMIT refreshRateChanged();
}

void RenderLoop::scheduleRepaint(Item *item)
{
    if (d->pendingRepaint || (d->fullscreenItem != nullptr && item != nullptr && item != d->fullscreenItem)) {
        return;
    }
    if (!d->pendingFrameCount && !d->inhibitCount) {
        d->scheduleRepaint();
    } else {
        d->delayScheduleRepaint();
    }
}

LatencyPolicy RenderLoop::latencyPolicy() const
{
    return d->latencyPolicy.value_or(options->latencyPolicy());
}

void RenderLoop::setLatencyPolicy(LatencyPolicy policy)
{
    d->latencyPolicy = policy;
}

void RenderLoop::resetLatencyPolicy()
{
    d->latencyPolicy.reset();
}

std::chrono::nanoseconds RenderLoop::lastPresentationTimestamp() const
{
    return d->lastPresentationTimestamp;
}

std::chrono::nanoseconds RenderLoop::nextPresentationTimestamp() const
{
    return d->nextPresentationTimestamp;
}

void RenderLoop::setFullscreenSurface(Item *surfaceItem)
{
    d->fullscreenItem = surfaceItem;
    if (SurfaceItemWayland *wayland = qobject_cast<SurfaceItemWayland *>(surfaceItem)) {
        d->allowTearing = d->canDoTearing && options->allowTearing() && wayland->surface()->presentationHint() == KWaylandServer::PresentationHint::Async;
    } else {
        d->allowTearing = false;
    }
}

RenderLoop::VrrPolicy RenderLoop::vrrPolicy() const
{
    return d->vrrPolicy;
}

void RenderLoop::setVrrPolicy(VrrPolicy policy)
{
    d->vrrPolicy = policy;
}

} // namespace KWin
