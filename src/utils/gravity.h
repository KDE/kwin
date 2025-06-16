/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QRectF>

namespace KWin
{

/**
 * The Gravity enum is used to specify the direction in which geometry changes during resize.
 */
class KWIN_EXPORT Gravity
{
public:
    enum Kind {
        None,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    Gravity();
    Gravity(Kind kind);

    operator Kind() const;

    QRectF apply(const QRectF &rect, const QRectF &bounds) const;

private:
    Kind m_kind;
};

} // namespace KWin
