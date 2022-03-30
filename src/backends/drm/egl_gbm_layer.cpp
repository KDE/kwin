/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_layer.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_buffer_gbm.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "egl_dmabuf.h"
#include "egl_gbm_backend.h"
#include "logging.h"
#include "surfaceitem_wayland.h"

#include "KWaylandServer/linuxdmabufv1clientbuffer.h"
#include "KWaylandServer/surface_interface.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend)
{
    connect(eglBackend, &EglGbmBackend::aboutToBeDestroyed, this, &EglGbmLayer::destroyResources);
}

EglGbmLayer::~EglGbmLayer()
{
    destroyResources();
}

void EglGbmLayer::destroyResources()
{
    m_surface.destroyResources();
}

OutputLayerBeginFrameInfo EglGbmLayer::beginFrame()
{
    m_scanoutBuffer.reset();
    // dmabuf feedback
    if (!m_scanoutCandidate.attemptedThisFrame && m_scanoutCandidate.surface) {
        if (const auto feedback = m_scanoutCandidate.surface->dmabufFeedbackV1()) {
            feedback->setTranches({});
        }
        m_scanoutCandidate.surface = nullptr;
    }
    m_scanoutCandidate.attemptedThisFrame = false;

    return m_surface.startRendering(m_pipeline->bufferSize(), m_pipeline->pending.sourceTransformation, m_pipeline->pending.bufferTransformation, m_pipeline->formats());
}

void EglGbmLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    m_surface.aboutToStartPainting(m_pipeline->output(), damagedRegion);
}

void EglGbmLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    const auto ret = m_surface.endRendering(m_pipeline->pending.bufferTransformation, damagedRegion);
    if (ret.has_value()) {
        std::tie(m_currentBuffer, m_currentDamage) = ret.value();
    }
}

QRegion EglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

QSharedPointer<DrmBuffer> EglGbmLayer::testBuffer()
{
    if (!m_surface.doesSurfaceFit(m_pipeline->sourceSize(), m_pipeline->formats())) {
        renderTestBuffer();
    }
    return m_currentBuffer;
}

bool EglGbmLayer::renderTestBuffer()
{
    const auto oldBuffer = m_currentBuffer;
    beginFrame();
    glClear(GL_COLOR_BUFFER_BIT);
    endFrame(QRegion(), infiniteRegion());
    return m_currentBuffer != oldBuffer;
}

QSharedPointer<GLTexture> EglGbmLayer::texture() const
{
    if (m_scanoutBuffer) {
        return m_scanoutBuffer->createTexture(m_surface.eglBackend()->eglDisplay());
    } else {
        return m_surface.texture();
    }
}

bool EglGbmLayer::scanout(SurfaceItem *surfaceItem)
{
    static bool valid;
    static const bool directScanoutDisabled = qEnvironmentVariableIntValue("KWIN_DRM_NO_DIRECT_SCANOUT", &valid) == 1 && valid;
    if (directScanoutDisabled) {
        return false;
    }

    SurfaceItemWayland *item = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!item || !item->surface()) {
        return false;
    }
    const auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(item->surface()->buffer());
    if (!buffer || buffer->planes().isEmpty() || buffer->size() != m_pipeline->sourceSize()) {
        return false;
    }

    if (m_scanoutCandidate.surface && m_scanoutCandidate.surface != item->surface() && m_scanoutCandidate.surface->dmabufFeedbackV1()) {
        m_scanoutCandidate.surface->dmabufFeedbackV1()->setTranches({});
    }
    m_scanoutCandidate.surface = item->surface();
    m_scanoutCandidate.attemptedThisFrame = true;

    if (!m_pipeline->formats().contains(buffer->format())) {
        sendDmabufFeedback(buffer);
        return false;
    }
    m_scanoutBuffer = QSharedPointer<DrmGbmBuffer>::create(m_pipeline->gpu(), buffer);
    if (!m_scanoutBuffer || !m_scanoutBuffer->bufferId()) {
        sendDmabufFeedback(buffer);
        m_scanoutBuffer.reset();
        return false;
    }

    if (m_pipeline->testScanout()) {
        m_currentBuffer = m_scanoutBuffer;
        m_currentDamage = surfaceItem->damage();
        surfaceItem->resetDamage();
        return true;
    } else {
        m_scanoutBuffer.reset();
        return false;
    }
}

void EglGbmLayer::sendDmabufFeedback(KWaylandServer::LinuxDmaBufV1ClientBuffer *failedBuffer)
{
    if (!m_scanoutCandidate.attemptedFormats[failedBuffer->format()].contains(failedBuffer->planes().first().modifier)) {
        m_scanoutCandidate.attemptedFormats[failedBuffer->format()] << failedBuffer->planes().first().modifier;
    }
    if (m_scanoutCandidate.surface->dmabufFeedbackV1()) {
        QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> scanoutTranches;
        const auto drmFormats = m_pipeline->formats();
        const auto tranches = m_surface.eglBackend()->dmabuf()->tranches();
        for (const auto &tranche : tranches) {
            KWaylandServer::LinuxDmaBufV1Feedback::Tranche scanoutTranche;
            for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
                const uint32_t format = it.key();
                const auto trancheModifiers = it.value();
                const auto drmModifiers = drmFormats[format];
                for (const auto &mod : trancheModifiers) {
                    if (drmModifiers.contains(mod) && !m_scanoutCandidate.attemptedFormats[format].contains(mod)) {
                        scanoutTranche.formatTable[format] << mod;
                    }
                }
            }
            if (!scanoutTranche.formatTable.isEmpty()) {
                scanoutTranche.device = m_pipeline->gpu()->deviceId();
                scanoutTranche.flags = KWaylandServer::LinuxDmaBufV1Feedback::TrancheFlag::Scanout;
                scanoutTranches << scanoutTranche;
            }
        }
        m_scanoutCandidate.surface->dmabufFeedbackV1()->setTranches(scanoutTranches);
    }
}

QSharedPointer<DrmBuffer> EglGbmLayer::currentBuffer() const
{
    return m_scanoutBuffer ? m_scanoutBuffer : m_currentBuffer;
}

bool EglGbmLayer::hasDirectScanoutBuffer() const
{
    return m_scanoutBuffer != nullptr;
}

}
