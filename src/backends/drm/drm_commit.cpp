/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_commit.h"
#include "drm_blob.h"
#include "drm_buffer.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_object.h"
#include "drm_property.h"

#include <QApplication>
#include <QThread>

namespace KWin
{

DrmCommit::DrmCommit(DrmGpu *gpu)
    : m_gpu(gpu)
{
}

DrmCommit::~DrmCommit()
{
}

DrmGpu *DrmCommit::gpu() const
{
    return m_gpu;
}

DrmAtomicCommit::DrmAtomicCommit(DrmGpu *gpu)
    : DrmCommit(gpu)
    , m_req(drmModeAtomicAlloc())
{
}

DrmAtomicCommit::~DrmAtomicCommit()
{
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());
    if (m_commitSuccessful) {
        for (const auto &[plane, buffer] : m_buffers) {
            plane->setCurrentBuffer(buffer);
        }
    }
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

void DrmAtomicCommit::addBuffer(DrmPlane *plane, const std::shared_ptr<DrmFramebuffer> &buffer)
{
    addProperty(plane->fbId, buffer ? buffer->framebufferId() : 0);
    m_buffers[plane] = buffer;
}

bool DrmAtomicCommit::test()
{
    return drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_NONBLOCK, this) == 0;
}

bool DrmAtomicCommit::testAllowModeset()
{
    return drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, this) == 0;
}

bool DrmAtomicCommit::commit()
{
    m_commitSuccessful = (drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, this) == 0);
    return m_commitSuccessful;
}

bool DrmAtomicCommit::commitModeset()
{
    m_commitSuccessful = (drmModeAtomicCommit(m_gpu->fd(), m_req.get(), DRM_MODE_ATOMIC_ALLOW_MODESET, this) == 0);
    return m_commitSuccessful;
}

drmModeAtomicReq *DrmAtomicCommit::req() const
{
    return m_req.get();
}

DrmLegacyCommit::DrmLegacyCommit(DrmCrtc *crtc, const std::shared_ptr<DrmFramebuffer> &buffer)
    : DrmCommit(crtc->gpu())
    , m_crtc(crtc)
    , m_buffer(buffer)
{
}

DrmLegacyCommit::~DrmLegacyCommit()
{
    if (m_success) {
        m_crtc->setCurrent(m_buffer);
    }
}

bool DrmLegacyCommit::doModeset(DrmConnector *connector, DrmConnectorMode *mode)
{
    uint32_t connectorId = connector->id();
    m_success = (drmModeSetCrtc(gpu()->fd(), m_crtc->id(), m_buffer->framebufferId(), 0, 0, &connectorId, 1, mode->nativeMode()) == 0);
    return m_success;
}

bool DrmLegacyCommit::doPageflip(uint32_t flags)
{
    m_success = (drmModePageFlip(gpu()->fd(), m_crtc->id(), m_buffer->framebufferId(), flags, this) == 0);
    return m_success;
}

}
