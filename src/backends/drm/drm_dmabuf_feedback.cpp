/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_dmabuf_feedback.h"

#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "egl_dmabuf.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/surface_interface.h"

namespace KWin
{

DmabufFeedback::DmabufFeedback(DrmGpu *gpu, EglGbmBackend *eglBackend)
    : m_gpu(gpu)
    , m_eglBackend(eglBackend)
{
}

void DmabufFeedback::renderingSurface()
{
    if (m_surface && !m_attemptedThisFrame) {
        if (const auto &feedback = m_surface->dmabufFeedbackV1()) {
            feedback->setTranches({});
        }
        m_surface = nullptr;
    }
    m_attemptedThisFrame = false;
}

void DmabufFeedback::scanoutSuccessful(KWaylandServer::SurfaceInterface *surface)
{
    if (surface != m_surface) {
        if (m_surface && m_surface->dmabufFeedbackV1()) {
            m_surface->dmabufFeedbackV1()->setTranches({});
        }
        m_surface = surface;
        m_attemptedFormats = {};
    }
}

void DmabufFeedback::scanoutFailed(KWaylandServer::SurfaceInterface *surface, const QMap<uint32_t, QVector<uint64_t>> &formats)
{
    m_attemptedThisFrame = true;
    if (surface != m_surface) {
        m_attemptedFormats = {};
        if (m_surface && m_surface->dmabufFeedbackV1()) {
            m_surface->dmabufFeedbackV1()->setTranches({});
        }
        m_surface = surface;
    }
    if (const auto &feedback = m_surface->dmabufFeedbackV1()) {
        const auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(surface->buffer());
        Q_ASSERT(buffer);
        const DmaBufAttributes &dmabufAttrs = buffer->attributes();
        if (!m_attemptedFormats[dmabufAttrs.format].contains(dmabufAttrs.modifier)) {
            m_attemptedFormats[dmabufAttrs.format] << dmabufAttrs.modifier;
            QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> scanoutTranches;
            const auto tranches = m_eglBackend->dmabuf()->tranches();
            for (const auto &tranche : tranches) {
                KWaylandServer::LinuxDmaBufV1Feedback::Tranche scanoutTranche;
                for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
                    const uint32_t format = it.key();
                    const auto trancheModifiers = it.value();
                    const auto drmModifiers = formats[format];
                    for (const auto &mod : trancheModifiers) {
                        if (drmModifiers.contains(mod) && !m_attemptedFormats[format].contains(mod)) {
                            scanoutTranche.formatTable[format] << mod;
                        }
                    }
                }
                if (!scanoutTranche.formatTable.isEmpty()) {
                    scanoutTranche.device = m_gpu->deviceId();
                    scanoutTranche.flags = KWaylandServer::LinuxDmaBufV1Feedback::TrancheFlag::Scanout;
                    scanoutTranches << scanoutTranche;
                }
            }
            feedback->setTranches(scanoutTranches);
        }
    }
}

}
