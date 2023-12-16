/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_abstract_output.h"
#include "core/renderbackend.h"
#include "core/renderloop_p.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"

namespace KWin
{

DrmAbstractOutput::DrmAbstractOutput(DrmGpu *gpu)
    : Output(gpu->platform())
    , m_renderLoop(std::make_unique<RenderLoop>())
    , m_gpu(gpu)
{
}

RenderLoop *DrmAbstractOutput::renderLoop() const
{
    return m_renderLoop.get();
}

void DrmAbstractOutput::frameFailed() const
{
    m_frame->failed();
}

void DrmAbstractOutput::pageFlipped(std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    m_frame->presented(std::chrono::nanoseconds(1'000'000'000'000 / refreshRate()), timestamp, mode);
    m_frame.reset();
}

DrmGpu *DrmAbstractOutput::gpu() const
{
    return m_gpu;
}

void DrmAbstractOutput::updateEnabled(bool enabled)
{
    State next = m_state;
    next.enabled = enabled;
    setState(next);
}

}

#include "moc_drm_abstract_output.cpp"
