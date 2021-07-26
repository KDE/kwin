/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpainterframeprofiler.h"

namespace KWin
{

void QPainterFrameProfiler::begin()
{
    m_start = std::chrono::steady_clock::now().time_since_epoch();
}

void QPainterFrameProfiler::end()
{
    m_end = std::chrono::steady_clock::now().time_since_epoch();
}

std::chrono::nanoseconds QPainterFrameProfiler::result()
{
    return m_end - m_start;
}

} // namespace KWin
