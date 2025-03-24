/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

template<typename T, typename M>
static inline constexpr ptrdiff_t offsetOf(const M T::*member)
{
    static_assert(std::is_standard_layout<T>::value, "pod type expected");
    return reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<T *>(0)->*member));
}

template<typename T, typename M>
static inline constexpr T *containerOf(M *ptr, const M T::*member)
{
    static_assert(std::is_standard_layout<T>::value, "pod type expected");
    return reinterpret_cast<T *>(reinterpret_cast<intptr_t>(ptr) - offsetOf(member));
}
