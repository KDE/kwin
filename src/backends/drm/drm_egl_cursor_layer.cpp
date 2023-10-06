/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_cursor_layer.h"
#include "drm_buffer.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"

#include <gbm.h>

namespace KWin
{

static TextureTransforms drmToTextureRotation(DrmPipeline *pipeline)
{
    auto angle = DrmPlane::transformationToDegrees(pipeline->renderOrientation());
    if (angle < 0) {
        angle += 360;
    }
    TextureTransforms flip = (pipeline->renderOrientation() & DrmPlane::Transformation::ReflectX) ? TextureTransform::MirrorX : TextureTransforms();
    switch (angle % 360) {
    case 0:
        return TextureTransforms() | flip;
    case 90:
        return TextureTransforms(TextureTransform::Rotate90) | flip;
    case 180:
        return TextureTransforms(TextureTransform::Rotate180) | flip;
    case 270:
        return TextureTransforms(TextureTransform::Rotate270) | flip;
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
    if (m_pipeline->output()->needsColormanagement()) {
        // TODO for hardware cursors to work with color management, KWin needs to offload post-blending color management steps to KMS
        return std::nullopt;
    }
    return m_surface.startRendering(m_pipeline->gpu()->cursorSize(), drmToTextureRotation(m_pipeline) | TextureTransform::MirrorY, m_pipeline->cursorFormats(), m_pipeline->colorDescription(), m_pipeline->output()->channelFactors(), m_pipeline->output()->needsColormanagement());
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

quint32 EglGbmCursorLayer::format() const
{
    return m_surface.currentBuffer()->buffer()->dmabufAttributes()->format;
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
