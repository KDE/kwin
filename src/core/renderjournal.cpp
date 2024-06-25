/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderjournal.h"

#include <algorithm>
#include <cmath>

using namespace std::chrono_literals;

namespace KWin
{

RenderJournal::RenderJournal()
{
}

static std::chrono::nanoseconds mix(std::chrono::nanoseconds duration1, std::chrono::nanoseconds duration2, double ratio)
{
    return std::chrono::nanoseconds(int64_t(std::round(duration1.count() * ratio + duration2.count() * (1 - ratio))));
}

void RenderJournal::add(std::chrono::nanoseconds renderTime, std::chrono::nanoseconds presentationTimestamp)
{
    const auto timeDifference = m_lastAdd ? presentationTimestamp - *m_lastAdd : 10s;
    m_lastAdd = presentationTimestamp;

    static constexpr std::chrono::nanoseconds varianceTimeConstant = 6s;
    const double varianceRatio = std::clamp(timeDifference.count() / double(varianceTimeConstant.count()), 0.001, 0.1);
    const auto renderTimeDiff = std::max(renderTime - m_result, 0ns);
    m_variance = std::max(mix(renderTimeDiff, m_variance, varianceRatio), renderTimeDiff);

    static constexpr std::chrono::nanoseconds timeConstant = 500ms;
    const double ratio = std::clamp(timeDifference.count() / double(timeConstant.count()), 0.01, 1.0);
    m_result = mix(renderTime, m_result, ratio);
}

std::chrono::nanoseconds RenderJournal::result() const
{
    return m_result + m_variance * 2;
}

} // namespace KWin
