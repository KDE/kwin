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

namespace KWin
{

DrmCrtc::DrmCrtc(uint32_t crtc_id, DrmBackend *backend, int resIndex)
    : DrmObject(crtc_id, backend->fd()),
      m_resIndex(resIndex),
      m_backend(backend)
{
    DrmScopedPointer<drmModeCrtc> modeCrtc(drmModeGetCrtc(backend->fd(), crtc_id));
    if (modeCrtc) {
        m_gammaRampSize = modeCrtc->gamma_size;
    }
}

DrmCrtc::~DrmCrtc()
{
}

bool DrmCrtc::atomicInit()
{
    qCDebug(KWIN_DRM) << "Atomic init for CRTC:" << resIndex() << "id:" << m_id;

    if (!initProps()) {
        return false;
    }
    return true;
}

bool DrmCrtc::initProps()
{
    setPropertyNames({
        QByteArrayLiteral("MODE_ID"),
        QByteArrayLiteral("ACTIVE"),
    });

    DrmScopedPointer<drmModeObjectProperties> properties(
        drmModeObjectGetProperties(fd(), m_id, DRM_MODE_OBJECT_CRTC));
    if (!properties) {
        qCWarning(KWIN_DRM) << "Failed to get properties for crtc " << m_id ;
        return false;
    }

    int propCount = int(PropertyIndex::Count);
    for (int j = 0; j < propCount; ++j) {
        initProp(j, properties.data());
    }

    return true;
}

void DrmCrtc::flipBuffer()
{
    if (m_currentBuffer && m_backend->deleteBufferAfterPageFlip() && m_currentBuffer != m_nextBuffer) {
        delete m_currentBuffer;
    }
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;

    delete m_blackBuffer;
    m_blackBuffer = nullptr;
}

bool DrmCrtc::blank()
{
    if (!m_output) {
        return false;
    }

    if (m_backend->atomicModeSetting()) {
        return false;
    }

    if (!m_blackBuffer) {
        DrmDumbBuffer *blackBuffer = m_backend->createBuffer(m_output->pixelSize());
        if (!blackBuffer->map()) {
            delete blackBuffer;
            return false;
        }
        blackBuffer->image()->fill(Qt::black);
        m_blackBuffer = blackBuffer;
    }

    if (m_output->setModeLegacy(m_blackBuffer)) {
        if (m_currentBuffer && m_backend->deleteBufferAfterPageFlip()) {
            delete m_currentBuffer;
            delete m_nextBuffer;
        }
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

    const bool isError = drmModeCrtcSetGamma(m_backend->fd(), m_id,
        gamma.size(), red, green, blue);

    return !isError;
}

}
