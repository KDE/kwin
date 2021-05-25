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
        PropertyDefinition(QByteArrayLiteral("VRR_ENABLED")),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT")),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT_SIZE")),
    }, DRM_MODE_OBJECT_CRTC);
}

void DrmCrtc::flipBuffer()
{
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;

    delete m_blackBuffer;
    m_blackBuffer = nullptr;
}

bool DrmCrtc::setGammaRamp(const GammaRamp &gamma)
{
    uint16_t *red = const_cast<uint16_t *>(gamma.red());
    uint16_t *green = const_cast<uint16_t *>(gamma.green());
    uint16_t *blue = const_cast<uint16_t *>(gamma.blue());

    const bool isError = drmModeCrtcSetGamma(gpu()->fd(), id(),
        gamma.size(), red, green, blue);

    return !isError;
}

bool DrmCrtc::setVrr(bool enable)
{
    if (const auto &prop = m_props[static_cast<int>(PropertyIndex::VrrEnabled)]) {
        if (prop->pending() == enable) {
            return false;
        }
        prop->setPending(enable);
        if (!gpu()->atomicModeSetting() || gpu()->useEglStreams()) {
            if (drmModeObjectSetProperty(gpu()->fd(), id(), DRM_MODE_OBJECT_CRTC, prop->propId(), enable) != 0) {
                qCWarning(KWIN_DRM) << "drmModeObjectSetProperty(VRR_ENABLED) failed";
                return false;
            }
        }
        return true;
    }
    return false;
}

bool DrmCrtc::isVrrEnabled() const
{
    if (const auto &prop = m_props[static_cast<int>(PropertyIndex::VrrEnabled)]) {
        return prop->pending();
    }
    return false;
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
