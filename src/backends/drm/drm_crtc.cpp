/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_crtc.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_commit.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pointer.h"

namespace KWin
{

DrmCrtc::DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane, DrmPlane *cursorPlane, DrmPlane *overlayPlane)
    : DrmObject(gpu, crtcId, DRM_MODE_OBJECT_CRTC)
    , modeId(this, QByteArrayLiteral("MODE_ID"))
    , active(this, QByteArrayLiteral("ACTIVE"))
    , vrrEnabled(this, QByteArrayLiteral("VRR_ENABLED"))
    , gammaLut(this, QByteArrayLiteral("GAMMA_LUT"))
    , gammaLutSize(this, QByteArrayLiteral("GAMMA_LUT_SIZE"))
    , ctm(this, QByteArrayLiteral("CTM"))
    , degammaLut(this, QByteArrayLiteral("DEGAMMA_LUT"))
    , degammaLutSize(this, QByteArrayLiteral("DEGAMMA_LUT_SIZE"))
    , m_crtc(drmModeGetCrtc(gpu->fd(), crtcId))
    , m_pipeIndex(pipeIndex)
    , m_primaryPlane(primaryPlane)
    , m_cursorPlane(cursorPlane)
    , m_overlayPlane(overlayPlane)
{
}

void DrmCrtc::setOverlayPlane(DrmPlane *plane)
{
    m_overlayPlane = plane;
}

bool DrmCrtc::init()
{
    return updateProperties();
}

bool DrmCrtc::updateProperties()
{
    if (!m_crtc) {
        return false;
    }
    DrmPropertyList props = queryProperties();
    modeId.update(props);
    active.update(props);
    vrrEnabled.update(props);
    gammaLut.update(props);
    gammaLutSize.update(props);
    ctm.update(props);
    degammaLut.update(props);
    degammaLutSize.update(props);

    if (!postBlendingPipeline) {
        DrmAbstractColorOp *next = nullptr;
        if (gammaLut.isValid() && gammaLutSize.isValid() && gammaLutSize.value() > 0) {
            m_postBlendingColorOps.push_back(std::make_unique<DrmLutColorOp>(next, &gammaLut, gammaLutSize.value()));
            next = m_postBlendingColorOps.back().get();
        }
        if (!gpu()->isNVidia() && ctm.isValid()) {
            m_postBlendingColorOps.push_back(std::make_unique<LegacyMatrixColorOp>(next, &ctm));
            next = m_postBlendingColorOps.back().get();
        }
        if (!gpu()->isNVidia() && !gpu()->isI915() && degammaLut.isValid() && degammaLutSize.isValid() && degammaLutSize.value() > 0) {
            m_postBlendingColorOps.push_back(std::make_unique<DrmLutColorOp>(next, &degammaLut, degammaLutSize.value()));
            next = m_postBlendingColorOps.back().get();
        }
        postBlendingPipeline = next;
    }

    const bool ret = !gpu()->atomicModeSetting() || (modeId.isValid() && active.isValid());
    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to update the basic crtc properties. modeId:" << modeId.isValid() << "active:" << active.isValid();
    }
    return ret;
}

drmModeModeInfo DrmCrtc::queryCurrentMode()
{
    DrmUniquePtr<drmModeCrtc> crtc(drmModeGetCrtc(gpu()->fd(), id()));
    if (crtc) {
        return crtc->mode;
    } else {
        return m_crtc->mode;
    }
}

int DrmCrtc::pipeIndex() const
{
    return m_pipeIndex;
}

std::shared_ptr<DrmFramebuffer> DrmCrtc::current() const
{
    return m_currentBuffer;
}

void DrmCrtc::setCurrent(const std::shared_ptr<DrmFramebuffer> &buffer)
{
    m_currentBuffer = buffer;
}

int DrmCrtc::gammaRampSize() const
{
    if (gpu()->atomicModeSetting()) {
        // limit atomic gamma ramp to 4096 to work around https://gitlab.freedesktop.org/drm/intel/-/issues/3916
        if (gammaLutSize.isValid() && gammaLutSize.value() <= 4096) {
            return gammaLutSize.value();
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

DrmPlane *DrmCrtc::overlayPlane() const
{
    return m_overlayPlane;
}

void DrmCrtc::disable(DrmAtomicCommit *commit)
{
    commit->addProperty(active, 0);
    commit->addProperty(modeId, 0);
}

void DrmCrtc::releaseCurrentBuffer()
{
    if (m_currentBuffer) {
        m_currentBuffer->releaseBuffer();
    }
}
}
