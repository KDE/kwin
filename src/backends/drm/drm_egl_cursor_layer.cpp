/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_cursor_layer.h"
#include "core/iccprofile.h"
#include "drm_buffer.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"

#include <gbm.h>

namespace KWin
{

static OutputTransform drmToOutputTransform(DrmPipeline *pipeline)
{
    auto angle = DrmPlane::transformationToDegrees(pipeline->renderOrientation());
    if (angle < 0) {
        angle += 360;
    }
    OutputTransform flip = (pipeline->renderOrientation() & DrmPlane::Transformation::ReflectX) ? OutputTransform::FlipX : OutputTransform();
    switch (angle % 360) {
    case 0:
        return flip;
    case 90:
        return flip.combine(OutputTransform::Rotate90);
    case 180:
        return flip.combine(OutputTransform::Rotate180);
    case 270:
        return flip.combine(OutputTransform::Rotate270);
    default:
        Q_UNREACHABLE();
    }
}

EglGbmCursorLayer::EglGbmCursorLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend, pipeline->gpu()->atomicModeSetting() ? EglGbmLayerSurface::BufferTarget::Linear : EglGbmLayerSurface::BufferTarget::Dumb, EglGbmLayerSurface::FormatOption::RequireAlpha)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmCursorLayer::beginFrame()
{
    // note that this allows blending to happen in sRGB or PQ encoding.
    // That's technically incorrect, but it looks okay and is intentionally allowed
    // as the hardware cursor is more important than an incorrectly blended cursor edge
    return m_surface.startRendering(m_pipeline->gpu()->cursorSize(), drmToOutputTransform(m_pipeline).combine(OutputTransform::FlipY), m_pipeline->cursorFormats(), m_pipeline->colorDescription(), m_pipeline->output()->channelFactors(), m_pipeline->iccProfile(), m_pipeline->output()->needsColormanagement());
}

bool EglGbmCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return m_surface.endRendering(damagedRegion);
}

QRegion EglGbmCursorLayer::currentDamage() const
{
    return {};
}

std::shared_ptr<DrmFramebuffer> EglGbmCursorLayer::currentBuffer() const
{
    return m_surface.currentBuffer();
}

bool EglGbmCursorLayer::checkTestBuffer()
{
    return false;
}

void EglGbmCursorLayer::releaseBuffers()
{
    m_surface.destroyResources();
}

std::chrono::nanoseconds EglGbmCursorLayer::queryRenderTime() const
{
    return m_surface.queryRenderTime();
}

std::optional<QSize> EglGbmCursorLayer::fixedSize() const
{
    return m_pipeline->gpu()->cursorSize();
}
}
