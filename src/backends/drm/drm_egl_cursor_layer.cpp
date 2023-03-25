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
    switch (angle % 360) {
    case 0:
        return TextureTransforms();
    case 90:
        return TextureTransform::Rotate90;
    case 180:
        return TextureTransform::Rotate180;
    case 270:
        return TextureTransform::Rotate270;
    default:
        Q_UNREACHABLE();
    }
}

EglGbmCursorLayer::EglGbmCursorLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmOverlayLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend, pipeline->gpu()->atomicModeSetting() ? EglGbmLayerSurface::BufferTarget::Linear : EglGbmLayerSurface::BufferTarget::Dumb, EglGbmLayerSurface::FormatOption::RequireAlpha)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmCursorLayer::beginFrame()
{
    return m_surface.startRendering(m_pipeline->gpu()->cursorSize(), drmToTextureRotation(m_pipeline) | TextureTransform::MirrorY, m_pipeline->cursorFormats());
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
    return m_surface.currentBuffer()->buffer()->format();
}
}
