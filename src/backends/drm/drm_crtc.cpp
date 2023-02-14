/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_crtc.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pointer.h"
#include <cerrno>

namespace KWin
{

DrmCrtc::DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane, DrmPlane *cursorPlane)
    : DrmObject(gpu, crtcId, {PropertyDefinition(QByteArrayLiteral("MODE_ID"), Requirement::Required), PropertyDefinition(QByteArrayLiteral("ACTIVE"), Requirement::Required), PropertyDefinition(QByteArrayLiteral("VRR_ENABLED"), Requirement::Optional), PropertyDefinition(QByteArrayLiteral("GAMMA_LUT"), Requirement::Optional), PropertyDefinition(QByteArrayLiteral("GAMMA_LUT_SIZE"), Requirement::Optional), PropertyDefinition(QByteArrayLiteral("CTM"), Requirement::Optional)}, DRM_MODE_OBJECT_CRTC)
    , m_crtc(drmModeGetCrtc(gpu->fd(), crtcId))
    , m_pipeIndex(pipeIndex)
    , m_primaryPlane(primaryPlane)
    , m_cursorPlane(cursorPlane)
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

int DrmCrtc::pipeIndex() const
{
    return m_pipeIndex;
}

std::shared_ptr<DrmFramebuffer> DrmCrtc::current() const
{
    return m_currentBuffer;
}

std::shared_ptr<DrmFramebuffer> DrmCrtc::next() const
{
    return m_nextBuffer;
}

void DrmCrtc::setCurrent(const std::shared_ptr<DrmFramebuffer> &buffer)
{
    m_currentBuffer = buffer;
}

void DrmCrtc::setNext(const std::shared_ptr<DrmFramebuffer> &buffer)
{
    m_nextBuffer = buffer;
}

int DrmCrtc::gammaRampSize() const
{
    if (gpu()->atomicModeSetting()) {
        // limit atomic gamma ramp to 4096 to work around https://gitlab.freedesktop.org/drm/intel/-/issues/3916
        if (auto prop = getProp(PropertyIndex::Gamma_LUT_Size); prop && prop->current() <= 4096) {
            return prop->current();
        }
    }
    return m_crtc->gamma_size;
}

DrmPlane *DrmCrtc::primaryPlane() const
{
    return m_primaryPlane;
}

DrmPlane *DrmCrtc::cursorPlane() const
{
    return m_cursorPlane;
}

void DrmCrtc::disable()
{
    setPending(PropertyIndex::Active, 0);
    setPending(PropertyIndex::ModeId, 0);
}

void DrmCrtc::releaseBuffers()
{
    if (m_nextBuffer) {
        m_nextBuffer->releaseBuffer();
    }
    if (m_currentBuffer) {
        m_currentBuffer->releaseBuffer();
    }
}
}
