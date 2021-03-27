/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

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

DrmCrtc::DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex)
    : DrmObject(gpu, crtcId)
    , m_crtc(drmModeGetCrtc(gpu->fd(), crtcId))
    , m_pipeIndex(pipeIndex)
{
}

bool DrmCrtc::init()
{
    if (!m_crtc) {
        return false;
    }
    qCDebug(KWIN_DRM) << "Init for CRTC:" << pipeIndex() << "id:" << id();
    return initProps({
        PropertyDefinition(QByteArrayLiteral("MODE_ID")),
        PropertyDefinition(QByteArrayLiteral("ACTIVE")),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT")),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT_SIZE")),
    }, DRM_MODE_OBJECT_CRTC);
}

void DrmCrtc::flipBuffer()
{
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;
}

drmModeModeInfo DrmCrtc::queryCurrentMode()
{
    m_crtc.reset(drmModeGetCrtc(gpu()->fd(), id()));
    return m_crtc->mode;
}

}
