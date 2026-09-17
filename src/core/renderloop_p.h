/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "renderbackend.h"
#include "renderjournal.h"
#include "renderloop.h"
#include "utils/precisetimer.h"

#include <QBasicTimer>

#include <fstream>
#include <optional>

namespace KWin
{

class SurfaceItem;
class OutputFrame;

class KWIN_EXPORT RenderLoopPrivate
{
public:
    static RenderLoopPrivate *get(RenderLoop *loop);
    explicit RenderLoopPrivate(RenderLoop *q, BackendOutput *output);

    void dispatch();

    void scheduleNextRepaint(std::optional<std::chrono::steady_clock::time_point> presentNotBefore);
    void scheduleRepaint(std::chrono::steady_clock::time_point lastTargetTimestamp, std::chrono::steady_clock::time_point presentNotBefore);

    void notifyFrameDropped();
    void notifyFrameCompleted(std::chrono::steady_clock::time_point timestamp, std::optional<RenderTimeSpan> renderTime, PresentationMode mode, OutputFrame *frame);
    void notifyVblank(std::chrono::steady_clock::time_point timestamp);

    RenderLoop *const q;
    BackendOutput *const output;
    std::optional<std::fstream> m_debugOutput;
    std::chrono::steady_clock::time_point lastPresentationTimestamp{};
    std::chrono::steady_clock::time_point nextPresentationTimestamp{};
    std::chrono::steady_clock::time_point lastPresentNotBefore{};
    bool wasTripleBuffering = false;
    int doubleBufferingCounter = 0;
    PreciseTimer compositeTimer;
    RenderJournal renderJournal;
    int refreshRate = 60000;
    int pendingFrameCount = 0;
    bool preparingNewFrame = false;
    int inhibitCount = 0;
    std::optional<std::chrono::steady_clock::time_point> pendingReschedule;
    std::chrono::nanoseconds safetyMargin{0};

    PresentationMode presentationMode = PresentationMode::VSync;
    int maxPendingFrameCount = 1;

    QBasicTimer delayedVrrTimer;
};

} // namespace KWin
