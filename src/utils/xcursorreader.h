/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/cursortheme.h"

namespace KWin
{

class XCursorReader
{
public:
    static QList<CursorSprite> load(const QString &filePath, int desiredSize, qreal devicePixelRatio);
};

} // namespace KWin
