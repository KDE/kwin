/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QRegion>

#include <limits>
#include <type_traits>

struct wl_resource;

namespace KWaylandServer
{

/**
 * Returns an infinite region.
 */
inline QRegion KWIN_EXPORT infiniteRegion()
{
    return QRegion(std::numeric_limits<int>::min() / 2, // "/ 2" is to avoid integer overflows
                   std::numeric_limits<int>::min() / 2,
                   std::numeric_limits<int>::max(),
                   std::numeric_limits<int>::max());
}

template<typename T>
T resource_cast(::wl_resource *resource)
{
    using ObjectType = std::remove_pointer_t<std::remove_cv_t<T>>;
    if (auto resourceContainer = ObjectType::Resource::fromResource(resource)) {
        return static_cast<T>(resourceContainer->object());
    }
    return T();
}

} // namespace KWaylandServer
