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

/**
 * The GraphicsBufferOptions describes the properties of an allocated graphics buffer.
 */
struct GraphicsBufferOptions
{
    /// The size of the buffer, in device pixels.
    QSize size;

    /// The pixel format of the buffer, see DRM_FORMAT_*.
    uint32_t format;

    /// An optional list of modifiers, see DRM_FORMAT_MOD_*.
    QVector<uint64_t> modifiers;

    /// Whether the graphics buffer should be suitable for software rendering.
    bool software = false;
};

class KWIN_EXPORT GraphicsBufferAllocator
{
public:
    GraphicsBufferAllocator();
    virtual ~GraphicsBufferAllocator();

    virtual GraphicsBuffer *allocate(const GraphicsBufferOptions &options) = 0;
};

} // namespace KWin
