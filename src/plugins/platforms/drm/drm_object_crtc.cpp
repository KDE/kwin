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

DrmCrtc::DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex)
    : DrmObject(gpu, crtcId, {
        PropertyDefinition(QByteArrayLiteral("MODE_ID")),
        PropertyDefinition(QByteArrayLiteral("ACTIVE")),
        PropertyDefinition(QByteArrayLiteral("VRR_ENABLED")),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT")),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT_SIZE")),
    }, DRM_MODE_OBJECT_CRTC)
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
    return initProps();
}

void DrmCrtc::flipBuffer()
{
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;

    delete m_blackBuffer;
    m_blackBuffer = nullptr;
}

drmModeModeInfo DrmCrtc::queryCurrentMode()
{
    m_crtc.reset(drmModeGetCrtc(gpu()->fd(), id()));
    return m_crtc->mode;
}

bool DrmCrtc::needsModeset() const
{
    return getProp(PropertyIndex::Active)->needsCommit()
        || getProp(PropertyIndex::ModeId)->needsCommit();
}

}
