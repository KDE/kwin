/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>

#include "renderjournal.h"

namespace KWin
{

RenderJournal::RenderJournal()
{
}

void RenderJournal::beginFrame()
{
    m_timer.start();
}

void RenderJournal::endFrame()
{
    std::chrono::nanoseconds duration(m_timer.nsecsElapsed());
    if (m_log.count() >= m_size) {
        m_log.dequeue();
    }
    m_log.enqueue(duration);
}

void RenderJournal::updateLastResultWithGpuTiming(std::chrono::nanoseconds gpuTime)
{
    qDebug() << "GPU: " << gpuTime.count() << "CPU: " << m_log.last().count();

    std::chrono::nanoseconds &last = m_log.last();
    last += gpuTime;
}

std::chrono::nanoseconds RenderJournal::minimum() const
{
    auto it = std::min_element(m_log.constBegin(), m_log.constEnd());
    return it != m_log.constEnd() ? (*it) : std::chrono::nanoseconds::zero();
}

std::chrono::nanoseconds RenderJournal::maximum() const
{
    auto it = std::max_element(m_log.constBegin(), m_log.constEnd());
    return it != m_log.constEnd() ? (*it) : std::chrono::nanoseconds::zero();
}

std::chrono::nanoseconds RenderJournal::average() const
{
    if (m_log.isEmpty()) {
        return std::chrono::nanoseconds::zero();
    }

    std::chrono::nanoseconds result = std::chrono::nanoseconds::zero();
    for (const std::chrono::nanoseconds &entry : m_log) {
        result += entry;
    }

    return result / m_log.count();
}

} // namespace KWin
