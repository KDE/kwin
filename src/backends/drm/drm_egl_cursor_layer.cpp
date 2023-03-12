/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_cursor_layer.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_pipeline.h"

#include <gbm.h>

namespace KWin
{

EglGbmCursorLayer::EglGbmCursorLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmOverlayLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend, pipeline->gpu()->atomicModeSetting() ? EglGbmLayerSurface::BufferTarget::Linear : EglGbmLayerSurface::BufferTarget::Dumb, EglGbmLayerSurface::FormatOption::RequireAlpha)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmCursorLayer::beginFrame()
{
    return m_surface.startRendering(m_pipeline->gpu()->cursorSize(), m_pipeline->renderOrientation(), DrmPlane::Transformation::Rotate0, m_pipeline->cursorFormats());
}

void EglGbmCursorLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    m_surface.aboutToStartPainting(m_pipeline->output(), damagedRegion);
}

bool EglGbmCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return m_surface.endRendering(m_pipeline->renderOrientation(), damagedRegion);
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

}
