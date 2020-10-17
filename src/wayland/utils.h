/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QRegion>

#include <limits>

namespace KWaylandServer
{

/**
 * Returns an infinite region.
 */
inline QRegion KWAYLANDSERVER_EXPORT infiniteRegion()
{
    return QRegion(std::numeric_limits<int>::min() / 2, // "/ 2" is to avoid integer overflows
                   std::numeric_limits<int>::min() / 2,
                   std::numeric_limits<int>::max(),
                   std::numeric_limits<int>::max());
}

} // namespace KWaylandServer
