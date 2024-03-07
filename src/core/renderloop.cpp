/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderloop.h"
#include "options.h"
#include "renderloop_p.h"
#include "scene/surfaceitem.h"
#include "utils/common.h"
#include "window.h"
#include "workspace.h"

#include <fstream>

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

RenderLoopPrivate::RenderLoopPrivate(RenderLoop *q, Output *output)
    : q(q)
    , output(output)
{
    compositeTimer.setSingleShot(true);
    QObject::connect(&compositeTimer, &QTimer::timeout, q, [this]() {
        dispatch();
    });
}

void RenderLoopPrivate::scheduleRepaint()
{
    if (kwinApp()->isTerminating() || compositeTimer.isActive()) {
        return;
    }
    const std::chrono::nanoseconds vblankInterval(1'000'000'000'000ull / refreshRate);
    const std::chrono::nanoseconds currentTime(std::chrono::steady_clock::now().time_since_epoch());

    // Estimate when the next presentation will occur. Note that this is a prediction.
    nextPresentationTimestamp = lastPresentationTimestamp + vblankInterval;
    if (nextPresentationTimestamp < currentTime && presentationMode == PresentationMode::VSync) {
        nextPresentationTimestamp = lastPresentationTimestamp
            + alignTimestamp(currentTime - lastPresentationTimestamp, vblankInterval);
    }

    // Estimate when it's a good time to perform the next compositing cycle.
    // the 1ms on top of the safety margin is required for timer and scheduler inaccuracies
    std::chrono::nanoseconds nextRenderTimestamp = nextPresentationTimestamp - renderJournal.result() - safetyMargin - std::chrono::milliseconds(1);

    // If we can't render the frame before the deadline, start compositing immediately.
    if (nextRenderTimestamp < currentTime) {
        nextRenderTimestamp = currentTime;
    }

    if (presentationMode == PresentationMode::Async || presentationMode == PresentationMode::AdaptiveAsync) {
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

void RenderLoopPrivate::notifyFrameCompleted(std::chrono::nanoseconds timestamp, std::chrono::nanoseconds renderTime, PresentationMode mode)
{
    Q_ASSERT(pendingFrameCount > 0);
    pendingFrameCount--;

    static std::fstream fs;
    if (!fs.is_open()) {
        fs.open("/home/vlad/log.csv", std::ios::out);
    }

    fs << (renderTime.count() / 1000000.0) << std::endl;
    fs.flush();

    notifyVblank(timestamp);

    renderJournal.add(renderTime, timestamp);
    if (!inhibitCount) {
        maybeScheduleRepaint();
    }

    Q_EMIT q->framePresented(q, timestamp, mode);
}

void RenderLoopPrivate::notifyVblank(std::chrono::nanoseconds timestamp)
{
    if (lastPresentationTimestamp <= timestamp) {
        lastPresentationTimestamp = timestamp;
    } else {
        qCDebug(KWIN_CORE,
                "Got invalid presentation timestamp: %lld (current %lld)",
                static_cast<long long>(timestamp.count()),
                static_cast<long long>(lastPresentationTimestamp.count()));
        lastPresentationTimestamp = std::chrono::steady_clock::now().time_since_epoch();
    }
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

RenderLoop::RenderLoop(Output *output)
    : d(std::make_unique<RenderLoopPrivate>(this, output))
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

void RenderLoop::prepareNewFrame()
{
    d->pendingFrameCount++;
}

void RenderLoop::beginPaint()
{
    d->pendingRepaint = false;
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

void RenderLoop::setPresentationSafetyMargin(std::chrono::nanoseconds safetyMargin)
{
    d->safetyMargin = safetyMargin;
}

void RenderLoop::scheduleRepaint(Item *item)
{
    if (d->pendingRepaint) {
        return;
    }
    const bool vrr = d->presentationMode == PresentationMode::AdaptiveSync || d->presentationMode == PresentationMode::AdaptiveAsync;
    if (vrr && workspace()->activeWindow() && d->output) {
        Window *const activeWindow = workspace()->activeWindow();
        if (activeWindow->isOnOutput(d->output) && activeWindow->surfaceItem() && item != activeWindow->surfaceItem() && activeWindow->surfaceItem()->frameTimeEstimation() <= std::chrono::nanoseconds(1'000'000'000) / 30) {
            return;
        }
    }
    if (!d->pendingFrameCount && !d->inhibitCount) {
        d->scheduleRepaint();
    } else {
        d->delayScheduleRepaint();
    }
}

std::chrono::nanoseconds RenderLoop::lastPresentationTimestamp() const
{
    return d->lastPresentationTimestamp;
}

std::chrono::nanoseconds RenderLoop::nextPresentationTimestamp() const
{
    return d->nextPresentationTimestamp;
}

void RenderLoop::setPresentationMode(PresentationMode mode)
{
    d->presentationMode = mode;
}

} // namespace KWin

#include "moc_renderloop.cpp"
