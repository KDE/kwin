/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"

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

    RectF apply(const RectF &rect, const RectF &bounds) const;

private:
    Kind m_kind;
};

} // namespace KWin
