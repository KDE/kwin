/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderjournal.h"

#include <algorithm>
#include <cmath>
#include <ranges>

using namespace std::chrono_literals;

namespace KWin
{

RenderJournal::RenderJournal()
{
}

void RenderJournal::add(std::chrono::nanoseconds renderTime, std::chrono::nanoseconds presentationTimestamp)
{
    m_history.push_back(renderTime);
    if (m_history.size() > 100) {
        m_history.pop_front();
    }
    const std::chrono::nanoseconds average = std::ranges::fold_left(m_history, 0ns, std::plus{}) / m_history.size();
    auto deviations = m_history | std::views::transform([&average](std::chrono::nanoseconds t) {
        const auto deviation = std::chrono::abs(t - average);
        return std::chrono::nanoseconds(deviation.count() * deviation.count());
    });
    const std::chrono::nanoseconds variance = std::ranges::fold_left(deviations, 0ns, std::plus{}) / std::max<size_t>(m_history.size() - 1, 1);
    const auto standardDeviation = std::chrono::nanoseconds{(int64_t)std::round(std::sqrt(variance.count()))};

    // 3x standard deviation above the mean includes the vast majority of values
    // in the history, without being 'overpowered' by individual bad frames
    m_result = average + 3 * standardDeviation;
}

std::chrono::nanoseconds RenderJournal::result() const
{
    return m_result;
}

} // namespace KWin
