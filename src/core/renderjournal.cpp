/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderjournal.h"

namespace KWin
{

RenderJournal::RenderJournal()
{
}

void RenderJournal::add(std::chrono::nanoseconds renderTime)
{
    if (renderTime > m_result || !m_lastAdd) {
        m_result = renderTime;
    } else {
        static constexpr std::chrono::nanoseconds timeConstant = std::chrono::milliseconds(500);
        const auto timeDifference = std::chrono::steady_clock::now() - *m_lastAdd;
        const double ratio = std::min(0.1, double(timeDifference.count()) / double(timeConstant.count()));
        m_result = std::chrono::nanoseconds(int64_t(renderTime.count() * ratio + m_result.count() * (1 - ratio)));
    }
    m_lastAdd = std::chrono::steady_clock::now();
}

std::chrono::nanoseconds RenderJournal::result() const
{
    return m_result;
}

} // namespace KWin
