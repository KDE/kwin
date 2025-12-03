/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace KWin
{

/**
 * The UInt32Serial type represents a uint32_t wrap-around serial.
 *
 * When comparing serials such as UINT32_MAX / 2 and 0 or UINT32_MAX, it is possible that both
 * cases a < b and b < a are true. The exact meaning depends on the context. Given how serials are
 * typically used, there will be little benefit from making such serials unordered.
 */
struct UInt32Serial
{
    uint32_t value = 0;

    constexpr UInt32Serial()
    {
    }

    constexpr UInt32Serial(uint32_t value)
        : value(value)
    {
    }

    constexpr std::weak_ordering operator<=>(const UInt32Serial &other) const
    {
        if (value == other.value) {
            return std::weak_ordering::equivalent;
        }

        if (value - other.value < std::numeric_limits<uint32_t>::max() / 2) {
            return std::weak_ordering::greater;
        } else {
            return std::weak_ordering::less;
        }
    }

    constexpr bool operator==(const UInt32Serial &other) const = default;
};

} // namespace KWin
