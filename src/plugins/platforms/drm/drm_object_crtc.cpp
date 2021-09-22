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
        PropertyDefinition(QByteArrayLiteral("MODE_ID"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("ACTIVE"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("VRR_ENABLED"), Requirement::Optional),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT"), Requirement::Optional),
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
    return initProps();
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

bool DrmCrtc::needsModeset() const
{
    return getProp(PropertyIndex::Active)->needsCommit()
        || getProp(PropertyIndex::ModeId)->needsCommit();
}

int DrmCrtc::pipeIndex() const
{
    return m_pipeIndex;
}

QSharedPointer<DrmBuffer> DrmCrtc::current() const
{
    return m_currentBuffer;
}
QSharedPointer<DrmBuffer> DrmCrtc::next() const
{
    return m_nextBuffer;
}
void DrmCrtc::setCurrent(const QSharedPointer<DrmBuffer> &buffer)
{
    m_currentBuffer = buffer;
}
void DrmCrtc::setNext(const QSharedPointer<DrmBuffer> &buffer)
{
    m_nextBuffer = buffer;
}

int DrmCrtc::gammaRampSize() const
{
    return m_crtc->gamma_size;
}

}
