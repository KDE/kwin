/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_cursor_layer.h"
#include "drm_gpu.h"
#include "drm_pipeline.h"
#include "egl_gbm_backend.h"

#include <gbm.h>

namespace KWin
{

EglGbmCursorLayer::EglGbmCursorLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmOverlayLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend)
{
}

OutputLayerBeginFrameInfo EglGbmCursorLayer::beginFrame()
{
    return m_surface.startRendering(m_pipeline->gpu()->cursorSize(), m_pipeline->renderOrientation(), DrmPlane::Transformation::Rotate0, m_pipeline->cursorFormats(), GBM_BO_USE_LINEAR);
}

void EglGbmCursorLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    m_surface.aboutToStartPainting(m_pipeline->output(), damagedRegion);
}

void EglGbmCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    const auto ret = m_surface.endRendering(m_pipeline->renderOrientation(), damagedRegion);
    if (ret.has_value()) {
        QRegion throwaway;
        std::tie(m_currentBuffer, throwaway) = ret.value();
    }
}

QRegion EglGbmCursorLayer::currentDamage() const
{
    return {};
}

std::shared_ptr<DrmFramebuffer> EglGbmCursorLayer::currentBuffer() const
{
    return m_currentBuffer;
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
