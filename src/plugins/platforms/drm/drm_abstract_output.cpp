/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_gpu.h"

namespace KWin
{
DrmAbstractOutput::DrmAbstractOutput(DrmGpu *gpu)
    : AbstractWaylandOutput(gpu->platform())
    , m_renderLoop(new RenderLoop(this))
    , m_gpu(gpu)
{
}

bool DrmAbstractOutput::initCursor(const QSize &cursorSize)
{
    Q_UNUSED(cursorSize)
    return true;
}

bool DrmAbstractOutput::showCursor()
{
    return true;
}

bool DrmAbstractOutput::hideCursor()
{
    return true;
}

bool DrmAbstractOutput::updateCursor()
{
    return true;
}

bool DrmAbstractOutput::moveCursor()
{
    return true;
}

DrmGpu *DrmAbstractOutput::gpu() const
{
    return m_gpu;
}

RenderLoop *DrmAbstractOutput::renderLoop() const
{
    return m_renderLoop;
}

}
