/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "drm_object_crtc.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_buffer.h"
#include "logging.h"
#include <colorcorrection/gammaramp.h>

namespace KWin
{

DrmCrtc::DrmCrtc(uint32_t crtc_id, DrmBackend *backend, int resIndex)
    : DrmObject(crtc_id, backend->fd()),
      m_resIndex(resIndex),
      m_backend(backend)
{
    ScopedDrmPointer<_drmModeCrtc, &drmModeFreeCrtc> modeCrtc(drmModeGetCrtc(backend->fd(), crtc_id));
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

    drmModeObjectProperties *properties = drmModeObjectGetProperties(fd(), m_id, DRM_MODE_OBJECT_CRTC);
    if (!properties) {
        qCWarning(KWIN_DRM) << "Failed to get properties for crtc " << m_id ;
        return false;
    }

    int propCount = int(PropertyIndex::Count);
    for (int j = 0; j < propCount; ++j) {
        initProp(j, properties);
    }
    drmModeFreeObjectProperties(properties);
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

bool DrmCrtc::setGammaRamp(ColorCorrect::GammaRamp &gamma) {
    bool isError = drmModeCrtcSetGamma(m_backend->fd(), m_id, gamma.size,
                                gamma.red, gamma.green, gamma.blue);
    return !isError;
}

}
