/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_lease_egl_gbm_layer.h"
#include "drm_buffer_gbm.h"
#include "egl_gbm_backend.h"
#include "drm_pipeline.h"
#include "drm_gpu.h"
#include "logging.h"

#include <drm_fourcc.h>
#include <gbm.h>

namespace KWin
{

DrmLeaseEglGbmLayer::DrmLeaseEglGbmLayer(EglGbmBackend *backend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
    connect(backend, &EglGbmBackend::aboutToBeDestroyed, this, [this]() {
        m_buffer.reset();
    });
}

QSharedPointer<DrmBuffer> DrmLeaseEglGbmLayer::testBuffer()
{
    const auto mods = m_pipeline->supportedModifiers(DRM_FORMAT_XRGB8888);
    const auto size = m_pipeline->sourceSize();
    if (!m_buffer || m_buffer->size() != size || !(mods.isEmpty() || mods.contains(m_buffer->modifier()))) {
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
        }
        if (newBo) {
            m_buffer = QSharedPointer<DrmGbmBuffer>::create(m_pipeline->gpu(), nullptr, newBo);
        } else {
            qCWarning(KWIN_DRM) << "Failed to create gbm_bo for lease output";
        }
    }
    return m_buffer;
}

QSharedPointer<DrmBuffer> DrmLeaseEglGbmLayer::currentBuffer() const
{
    return m_buffer;
}

}
