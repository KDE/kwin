/*
    SPDX-FileCopyrightText: 2024 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QString>

namespace KWin
{

/**
 * Any strings that come from user-defined sources that could exceed the maximum wayland length (e.g. xwayland clients)
 * need to be truncated to the maximumWaylandBufferSize.
 */
static inline QString truncate(const QString &stringIn)
{
    const int libwaylandMaxBufferSize = 4096;
    // Some parts of the buffer is used for metadata, so subtract 100 to be on the safe side.
    // Also, QString is in utf-16, which means that in the worst case each character will be
    // three bytes when converted to utf-8 (which is what libwayland uses), so divide by three.
    const int maxLength = libwaylandMaxBufferSize / 3 - 100;
    return stringIn.left(maxLength);
}

} // namespace KWin
