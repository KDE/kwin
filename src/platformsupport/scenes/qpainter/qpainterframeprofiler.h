/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <chrono>

namespace KWin
{

class KWIN_EXPORT QPainterFrameProfiler
{
public:
    void begin();
    void end();

    std::chrono::nanoseconds result();

private:
    std::chrono::nanoseconds m_start = std::chrono::nanoseconds::zero();
    std::chrono::nanoseconds m_end = std::chrono::nanoseconds::zero();
};

} // namespace KWin
