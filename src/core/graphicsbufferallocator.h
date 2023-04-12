/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QSize>
#include <QVector>

namespace KWin
{

class GraphicsBuffer;

class KWIN_EXPORT GraphicsBufferAllocator
{
public:
    GraphicsBufferAllocator();
    virtual ~GraphicsBufferAllocator();

    virtual GraphicsBuffer *allocate(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers = {}) = 0;
};

} // namespace KWin
