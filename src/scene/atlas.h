/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"

namespace KWin
{

class Atlas
{
public:
    virtual ~Atlas();

    struct Sprite
    {
        Rect geometry;
        bool rotated;
    };

    virtual Sprite sprite(uint spriteId) const = 0;
    virtual bool update(uint spriteId, const QImage &image, const Rect &damage) = 0;
    virtual bool reset(const QList<QImage> &images) = 0;

protected:
    Atlas();
};

} // namespace KWin
