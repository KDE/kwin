/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"
#include "options.h"

#include <QObject>

namespace KWin
{

class RenderLoopPrivate;
class Item;

/**
 * The RenderLoop class represents the compositing scheduler on a particular output.
 *
 * The RenderLoop class drives the compositing. The frameRequested() signal is emitted
 * when the loop wants a new frame to be rendered. The frameCompleted() signal is
 * emitted when a previously rendered frame has been presented on the screen. In case
 * you want the compositor to repaint the scene, call the scheduleRepaint() function.
 */
class KWIN_EXPORT RenderLoop : public QObject
{
    Q_OBJECT

public:
    explicit RenderLoop();
    ~RenderLoop() override;

    /**
     * Pauses the render loop. While the render loop is inhibited, scheduleRepaint()
     * requests are queued.
     *
     * Once the render loop is uninhibited, the pending schedule requests are going to
     * be re-applied.
     */
    void inhibit();

    /**
     * Uninhibits the render loop.
     */
    void uninhibit();

    /**
     * This function must be called before the Compositor starts rendering the next
     * frame.
     */
    void beginFrame();

    /**
     * This function must be called after the Compositor has finished rendering the
     * next frame.
     */
    void endFrame();

    /**
     * Returns the refresh rate at which the output is being updated, in millihertz.
     */
    int refreshRate() const;

    /**
     * Sets the refresh rate of this RenderLoop to @a refreshRate, in millihertz.
     */
    void setRefreshRate(int refreshRate);

    /**
     * Schedules a compositing cycle at the next available moment.
     */
    void scheduleRepaint(Item *item = nullptr);

    /**
     * Returns the timestamp of the last frame that has been presented on the screen.
     * The returned timestamp is sourced from the monotonic clock.
     */
    std::chrono::nanoseconds lastPresentationTimestamp() const;

    /**
     * If a repaint has been scheduled, this function returns the expected time when
     * the next frame will be presented on the screen. The returned timestamp is sourced
     * from the monotonic clock.
     */
    std::chrono::nanoseconds nextPresentationTimestamp() const;

    /**
     * Sets the surface that currently gets scanned out,
     * so that this RenderLoop can adjust its timing behavior to that surface
     */
    void setFullscreenSurface(Item *surface);

    enum class VrrPolicy : uint32_t {
        Never = 0,
        Always = 1,
        Automatic = 2,
    };
    Q_ENUM(VrrPolicy)

    /**
     * the current policy regarding the use of variable refresh rate
     */
    VrrPolicy vrrPolicy() const;

    /**
     * Set the policy regarding the use of variable refresh rate with RenderLoop
     */
    void setVrrPolicy(VrrPolicy vrrPolicy);

    /**
     * Returns the latency policy for this render loop.
     */
    LatencyPolicy latencyPolicy() const;

    /**
     * Sets the latecy policy of this render loop to @a policy. By default,
     * the latency policy of this render loop matches options->latencyPolicy().
     */
    void setLatencyPolicy(LatencyPolicy policy);

    /**
     * Resets the latency policy to the default value.
     */
    void resetLatencyPolicy();

Q_SIGNALS:
    /**
     * This signal is emitted when the refresh rate of this RenderLoop has changed.
     */
    void refreshRateChanged();
    /**
     * This signal is emitted when a frame has been actually presented on the screen.
     * @a timestamp indicates the time when it took place.
     */
    void framePresented(RenderLoop *loop, std::chrono::nanoseconds timestamp);

    /**
     * This signal is emitted when the render loop wants a new frame to be composited.
     *
     * The Compositor should make a connection to this signal using Qt::DirectConnection.
     */
    void frameRequested(RenderLoop *loop);

private:
    std::unique_ptr<RenderLoopPrivate> d;
    friend class RenderLoopPrivate;
};

} // namespace KWin
