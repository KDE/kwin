/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_abstract_output.h"
#include "drm_gpu.h"
#include "drm_backend.h"
#include "renderloop_p.h"

namespace KWin
{

DrmAbstractOutput::DrmAbstractOutput(DrmGpu *gpu)
    : AbstractWaylandOutput(gpu->platform())
    , DrmDisplayDevice(gpu)
    , m_renderLoop(new RenderLoop(this))
{
}

RenderLoop *DrmAbstractOutput::renderLoop() const
{
    return m_renderLoop;
}

QRect DrmAbstractOutput::renderGeometry() const
{
    return geometry();
}

void DrmAbstractOutput::frameFailed() const
{
    RenderLoopPrivate::get(m_renderLoop)->notifyFrameFailed();
}

void DrmAbstractOutput::pageFlipped(std::chrono::nanoseconds timestamp) const
{
    RenderLoopPrivate::get(m_renderLoop)->notifyFrameCompleted(timestamp);
}

}
