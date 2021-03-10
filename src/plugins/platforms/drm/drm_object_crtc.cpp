/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object_crtc.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_buffer.h"
#include "drm_pointer.h"
#include "logging.h"
#include "drm_gpu.h"

namespace KWin
{

DrmCrtc::DrmCrtc(uint32_t crtc_id, DrmBackend *backend, DrmGpu *gpu, int resIndex)
    : DrmObject(crtc_id, gpu->fd()),
      m_crtc(drmModeGetCrtc(gpu->fd(), crtc_id)),
      m_resIndex(resIndex),
      m_backend(backend),
      m_gpu(gpu)
{
}

bool DrmCrtc::init()
{
    if (!m_crtc) {
        return false;
    }
    qCDebug(KWIN_DRM) << "Init for CRTC:" << resIndex() << "id:" << m_id;
    return initProps({
        PropertyDefinition(QByteArrayLiteral("MODE_ID")),
        PropertyDefinition(QByteArrayLiteral("ACTIVE")),
    }, DRM_MODE_OBJECT_CRTC);
}

void DrmCrtc::flipBuffer()
{
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;

    delete m_blackBuffer;
    m_blackBuffer = nullptr;
}

bool DrmCrtc::blank(DrmOutput *output)
{
    if (m_gpu->atomicModeSetting()) {
        return false;
    }

    if (!m_blackBuffer) {
        DrmDumbBuffer *blackBuffer = new DrmDumbBuffer(m_gpu, output->pixelSize());
        if (!blackBuffer->map()) {
            delete blackBuffer;
            return false;
        }
        blackBuffer->image()->fill(Qt::black);
        m_blackBuffer = blackBuffer;
    }

    if (output->setModeLegacy(m_blackBuffer)) {
        m_currentBuffer = nullptr;
        m_nextBuffer = nullptr;
        return true;
    }
    return false;
}

bool DrmCrtc::setGammaRamp(const GammaRamp &gamma)
{
    uint16_t *red = const_cast<uint16_t *>(gamma.red());
    uint16_t *green = const_cast<uint16_t *>(gamma.green());
    uint16_t *blue = const_cast<uint16_t *>(gamma.blue());

    const bool isError = drmModeCrtcSetGamma(m_gpu->fd(), m_id,
        gamma.size(), red, green, blue);

    return !isError;
}

}
