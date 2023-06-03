/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <stdint.h>

namespace KWin
{

class DrmGpu;
class DrmFramebuffer;
class GraphicsBuffer;

class DrmFramebuffer
{
public:
    DrmFramebuffer(DrmGpu *gpu, uint32_t fbId, GraphicsBuffer *buffer);
    ~DrmFramebuffer();

    uint32_t framebufferId() const;
    /**
     * may be nullptr
     */
    GraphicsBuffer *buffer() const;

    void releaseBuffer();

protected:
    const uint32_t m_framebufferId;
    DrmGpu *const m_gpu;
    GraphicsBuffer *m_buffer = nullptr;
};

}
