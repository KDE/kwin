/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/xcursortheme.h"

namespace KWin
{

class SvgCursorReader
{
public:
    static QList<KXcursorSprite> load(const QString &filePath, int desiredSize, qreal devicePixelRatio);
};

} // namespace KWin
