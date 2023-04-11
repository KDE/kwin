/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_atomic_commit.h"
#include "drm_blob.h"
#include "drm_gpu.h"
#include "drm_object.h"
#include "drm_property.h"

namespace KWin
{

DrmAtomicCommit::DrmAtomicCommit(DrmGpu *gpu)
    : m_gpu(gpu)
    , m_req(drmModeAtomicAlloc())
{
}

void DrmAtomicCommit::addProperty(const DrmProperty &prop, uint64_t value)
{
    drmModeAtomicAddProperty(m_req.get(), prop.drmObject()->id(), prop.propId(), value);
}

void DrmAtomicCommit::addBlob(const DrmProperty &prop, const std::shared_ptr<DrmBlob> &blob)
{
    addProperty(prop, blob ? blob->blobId() : 0);
    m_blobs[&prop] = blob;
}

bool DrmAtomicCommit::test()
{
    return drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_NONBLOCK, m_gpu) == 0;
}

bool DrmAtomicCommit::testAllowModeset()
{
    return drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, m_gpu) == 0;
}

bool DrmAtomicCommit::commit()
{
    return drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, m_gpu) == 0;
}

bool DrmAtomicCommit::commitModeset()
{
    return drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_ALLOW_MODESET, m_gpu) == 0;
}

drmModeAtomicReq *DrmAtomicCommit::req() const
{
    return m_req.get();
}

}
