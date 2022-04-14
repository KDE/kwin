/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "renderloop_p.h"

namespace KWin
{

DrmAbstractOutput::DrmAbstractOutput(DrmGpu *gpu)
    : Output(gpu->platform())
    , m_renderLoop(new RenderLoop(this))
    , m_gpu(gpu)
{
}

RenderLoop *DrmAbstractOutput::renderLoop() const
{
    return m_renderLoop;
}

void DrmAbstractOutput::frameFailed() const
{
    RenderLoopPrivate::get(m_renderLoop)->notifyFrameFailed();
}

void DrmAbstractOutput::pageFlipped(std::chrono::nanoseconds timestamp) const
{
    RenderLoopPrivate::get(m_renderLoop)->notifyFrameCompleted(timestamp);
}

QVector<int32_t> DrmAbstractOutput::regionToRects(const QRegion &region) const
{
    const int height = pixelSize().height();
    const QMatrix4x4 matrix = Output::logicalToNativeMatrix(rect(), scale(), transform());
    QVector<EGLint> rects;
    rects.reserve(region.rectCount() * 4);
    for (const QRect &_rect : region) {
        const QRect rect = matrix.mapRect(_rect);
        rects << rect.left();
        rects << height - (rect.y() + rect.height());
        rects << rect.width();
        rects << rect.height();
    }
    return rects;
}

DrmGpu *DrmAbstractOutput::gpu() const
{
    return m_gpu;
}

}
