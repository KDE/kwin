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

#include <filesystem>

using namespace std::chrono_literals;

namespace KWin
{

RenderLoopPrivate *RenderLoopPrivate::get(RenderLoop *loop)
{
    return loop->d.get();
}

static const bool s_printDebugInfo = qEnvironmentVariableIntValue("KWIN_LOG_PERFORMANCE_DATA") != 0;

RenderLoopPrivate::RenderLoopPrivate(RenderLoop *q, Output *output)
    : q(q)
    , output(output)
{
    compositeTimer.setSingleShot(true);
    compositeTimer.setTimerType(Qt::PreciseTimer);

    QObject::connect(&compositeTimer, &QTimer::timeout, q, [this]() {
        dispatch();
    });

    delayedVrrTimer.setSingleShot(true);
    delayedVrrTimer.setInterval(1'000 / 30);
    delayedVrrTimer.setTimerType(Qt::PreciseTimer);

    QObject::connect(&delayedVrrTimer, &QTimer::timeout, q, [q]() {
        q->scheduleRepaint(nullptr, nullptr);
    });
}

void RenderLoopPrivate::scheduleNextRepaint()
{
    if (kwinApp()->isTerminating() || compositeTimer.isActive()) {
        return;
    }
    scheduleRepaint(nextPresentationTimestamp);
}

void RenderLoopPrivate::scheduleRepaint(std::chrono::nanoseconds lastTargetTimestamp)
{
    pendingReschedule = false;
    const std::chrono::nanoseconds vblankInterval(1'000'000'000'000ull / refreshRate);
    const std::chrono::nanoseconds currentTime(std::chrono::steady_clock::now().time_since_epoch());

    // Estimate when it's a good time to perform the next compositing cycle.
    // the 1ms on top of the safety margin is required for timer and scheduler inaccuracies
    std::chrono::nanoseconds expectedCompositingTime = std::min(renderJournal.result() + safetyMargin + 1ms, 2 * vblankInterval);

    if (presentationMode == PresentationMode::VSync) {
        // normal presentation: pageflips only happen at vblank
        const uint64_t pageflipsSince = std::max<int64_t>((currentTime - lastPresentationTimestamp) / vblankInterval, 0);
        if (pageflipsSince > 100) {
            // if it's been a while since the last frame, the GPU is likely in a low power state and render time will be increased
            // -> take that into account and start compositing very early
            expectedCompositingTime = std::max(vblankInterval - 1us, expectedCompositingTime);
        }
        const uint64_t pageflipsSinceLastToTarget = std::max<int64_t>(std::round((lastTargetTimestamp - lastPresentationTimestamp).count() / double(vblankInterval.count())), 0);
        uint64_t pageflipsInAdvance = std::min<int64_t>(expectedCompositingTime / vblankInterval + 1, maxPendingFrameCount);

        // switching from double to triple buffering causes a frame drop
        // -> apply some amount of hysteresis to avoid switching back and forth constantly
        if (pageflipsInAdvance > 1) {
            // immediately switch to triple buffering when needed
            wasTripleBuffering = true;
            doubleBufferingCounter = 0;
        } else if (wasTripleBuffering) {
            // but wait a bit before switching back to double buffering
            if (doubleBufferingCounter >= 10) {
                wasTripleBuffering = false;
            } else if (expectedCompositingTime >= vblankInterval * 0.95) {
                // also don't switch back if render times are just barely enough for double buffering
                pageflipsInAdvance = 2;
                doubleBufferingCounter = 0;
                expectedCompositingTime = vblankInterval;
            } else {
                doubleBufferingCounter++;
                pageflipsInAdvance = 2;
                expectedCompositingTime = vblankInterval;
            }
        }

        if (compositeTimer.isActive()) {
            // we already scheduled this frame, but we got a new timestamp
            // which might require starting to composite earlier than we planned
            // It's important here that we do not change the targeted vblank interval,
            // otherwise with a pessimistic compositing time estimation we might
            // unnecessarily drop frames
            const uint32_t intervalsSinceLastTimestamp = std::max<int32_t>(std::round((nextPresentationTimestamp - lastPresentationTimestamp).count() / double(vblankInterval.count())), 0);
            nextPresentationTimestamp = lastPresentationTimestamp + intervalsSinceLastTimestamp * vblankInterval;
        } else {
            nextPresentationTimestamp = lastPresentationTimestamp + std::max(pageflipsSince + pageflipsInAdvance, pageflipsSinceLastToTarget + 1) * vblankInterval;
        }
    } else {
        wasTripleBuffering = false;
        doubleBufferingCounter = 0;
        if (presentationMode == PresentationMode::Async || presentationMode == PresentationMode::AdaptiveAsync) {
            // tearing: pageflips happen ASAP
            nextPresentationTimestamp = currentTime;
        } else {
            // adaptive sync: pageflips happen after one vblank interval
            // TODO read minimum refresh rate from the EDID and take it into account here
            nextPresentationTimestamp = lastPresentationTimestamp + vblankInterval;
        }
    }

    const std::chrono::nanoseconds nextRenderTimestamp = nextPresentationTimestamp - expectedCompositingTime;
    compositeTimer.start(std::max(0ms, std::chrono::duration_cast<std::chrono::milliseconds>(nextRenderTimestamp - currentTime)));
}

void RenderLoopPrivate::delayScheduleRepaint()
{
    pendingReschedule = true;
}

void RenderLoopPrivate::notifyFrameDropped()
{
    Q_ASSERT(pendingFrameCount > 0);
    pendingFrameCount--;

    if (!inhibitCount && pendingReschedule) {
        scheduleNextRepaint();
    }
}

void RenderLoopPrivate::notifyFrameCompleted(std::chrono::nanoseconds timestamp, std::optional<RenderTimeSpan> renderTime, PresentationMode mode, OutputFrame *frame)
{
    if (output && s_printDebugInfo && !m_debugOutput) {
        m_debugOutput = std::fstream(qPrintable("kwin perf statistics " + output->name() + ".csv"), std::ios::out);
        *m_debugOutput << "target pageflip timestamp,pageflip timestamp,render start,render end,safety margin,refresh duration,vrr,tearing,predicted render time\n";
    }
    if (m_debugOutput) {
        auto times = renderTime.value_or(RenderTimeSpan{});
        const bool vrr = mode == PresentationMode::AdaptiveSync || mode == PresentationMode::AdaptiveAsync;
        const bool tearing = mode == PresentationMode::Async || mode == PresentationMode::AdaptiveAsync;
        *m_debugOutput << frame->targetPageflipTime().time_since_epoch().count() << "," << timestamp.count() << "," << times.start.time_since_epoch().count() << "," << times.end.time_since_epoch().count()
                       << "," << safetyMargin.count() << "," << frame->refreshDuration().count() << "," << (vrr ? 1 : 0) << "," << (tearing ? 1 : 0) << "," << frame->predictedRenderTime().count() << "\n";
    }

    Q_ASSERT(pendingFrameCount > 0);
    pendingFrameCount--;

    notifyVblank(timestamp);

    if (renderTime) {
        renderJournal.add(renderTime->end - renderTime->start, timestamp);
    }
    if (compositeTimer.isActive()) {
        // reschedule to match the new timestamp and render time
        scheduleRepaint(lastPresentationTimestamp);
    }
    if (!inhibitCount && pendingReschedule) {
        scheduleNextRepaint();
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
        d->scheduleNextRepaint();
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

void RenderLoop::scheduleRepaint(Item *item, RenderLayer *layer, OutputLayer *outputLayer)
{
    if (d->pendingRepaint) {
        return;
    }
    const bool vrr = d->presentationMode == PresentationMode::AdaptiveSync || d->presentationMode == PresentationMode::AdaptiveAsync;
    const bool tearing = d->presentationMode == PresentationMode::Async || d->presentationMode == PresentationMode::AdaptiveAsync;
    if ((vrr || tearing) && workspace()->activeWindow() && d->output) {
        Window *const activeWindow = workspace()->activeWindow();
        if ((item || layer || outputLayer) && activeWindow->isOnOutput(d->output) && activeWindow->surfaceItem() && item != activeWindow->surfaceItem() && activeWindow->surfaceItem()->frameTimeEstimation() <= std::chrono::nanoseconds(1'000'000'000) / 30) {
            d->delayedVrrTimer.start();
            return;
        }
    }
    d->delayedVrrTimer.stop();
    const int effectiveMaxPendingFrameCount = (vrr || tearing) ? 1 : d->maxPendingFrameCount;
    if (d->pendingFrameCount < effectiveMaxPendingFrameCount && !d->inhibitCount) {
        d->scheduleNextRepaint();
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
    if (mode != d->presentationMode) {
        qCDebug(KWIN_CORE) << "Changed presentation mode to" << mode;
    }
    d->presentationMode = mode;
}

void RenderLoop::setMaxPendingFrameCount(uint32_t maxCount)
{
    d->maxPendingFrameCount = maxCount;
}

std::chrono::nanoseconds RenderLoop::predictedRenderTime() const
{
    return d->renderJournal.result();
}

} // namespace KWin

#include "moc_renderloop.cpp"
