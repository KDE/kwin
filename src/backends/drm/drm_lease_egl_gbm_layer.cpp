/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_lease_egl_gbm_layer.h"
#include "drm_buffer.h"
#include "drm_buffer_gbm.h"
#include "drm_gpu.h"
#include "drm_pipeline.h"
#include "egl_gbm_backend.h"
#include "logging.h"

#include <drm_fourcc.h>
#include <gbm.h>

namespace KWin
{

DrmLeaseEglGbmLayer::DrmLeaseEglGbmLayer(DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
}

bool DrmLeaseEglGbmLayer::checkTestBuffer()
{
    const auto mods = m_pipeline->formats().value(DRM_FORMAT_XRGB8888);
    const auto size = m_pipeline->bufferSize();
    if (!m_framebuffer || m_framebuffer->buffer()->size() != size || !(mods.isEmpty() || mods.contains(m_framebuffer->buffer()->modifier()))) {
        gbm_bo *newBo;
        if (mods.isEmpty()) {
            newBo = gbm_bo_create(m_pipeline->gpu()->gbmDevice(), size.width(), size.height(), DRM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);
        } else {
            QVector<uint64_t> modifiers;
            modifiers.reserve(mods.count());
            for (const auto &mod : mods) {
                modifiers << mod;
            }
            newBo = gbm_bo_create_with_modifiers(m_pipeline->gpu()->gbmDevice(), size.width(), size.height(), DRM_FORMAT_XRGB8888, modifiers.constData(), mods.count());
            if (!newBo && errno == ENOSYS) {
                // gbm implementation doesn't support modifiers
                newBo = gbm_bo_create(m_pipeline->gpu()->gbmDevice(), size.width(), size.height(), DRM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);
            }
        }
        if (newBo) {
            m_framebuffer = DrmFramebuffer::createFramebuffer(std::make_shared<GbmBuffer>(m_pipeline->gpu(), newBo));
            if (!m_framebuffer) {
                qCWarning(KWIN_DRM, "Failed to create gbm framebuffer for lease output: %s", strerror(errno));
            }
        } else {
            qCWarning(KWIN_DRM) << "Failed to create gbm_bo for lease output";
        }
    }
    return m_framebuffer != nullptr;
}

std::shared_ptr<DrmFramebuffer> DrmLeaseEglGbmLayer::currentBuffer() const
{
    return m_framebuffer;
}

OutputLayerBeginFrameInfo DrmLeaseEglGbmLayer::beginFrame()
{
    return {};
}

bool DrmLeaseEglGbmLayer::endFrame(const QRegion &damagedRegion, const QRegion &renderedRegion)
{
    Q_UNUSED(damagedRegion)
    Q_UNUSED(renderedRegion)
    return false;
}

void DrmLeaseEglGbmLayer::releaseBuffers()
{
    m_framebuffer.reset();
}
}
