/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QElapsedTimer>
#include <QQueue>

namespace KWin
{

/**
 * The RenderJournal class measures how long it takes to render frames and estimates how
 * long it will take to render the next frame.
 */
class KWIN_EXPORT RenderJournal
{
public:
    RenderJournal();

    /**
     * This function must be called before starting rendering a new frame.
     */
    void beginFrame();

    /**
     * This function must be called after finishing rendering a frame.
     */
    void endFrame();

    /**
     * Returns the maximum estimated amount of time that it takes to render a single frame.
     */
    std::chrono::nanoseconds maximum() const;

    /**
     * Returns the minimum estimated amount of time that it takes to render a single frame.
     */
    std::chrono::nanoseconds minimum() const;

    /**
     * Returns the average estimated amount of time that it takes to render a single frame.
     */
    std::chrono::nanoseconds average() const;

private:
    QElapsedTimer m_timer;
    QQueue<std::chrono::nanoseconds> m_log;
    int m_size = 15;
};

} // namespace KWin
