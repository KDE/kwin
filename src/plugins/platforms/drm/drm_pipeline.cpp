/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include "logging.h"

#include "drm_gpu.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_buffer.h"
#include "cursor.h"
#include "session.h"
#include "abstract_output.h"

#include <errno.h>

namespace KWin
{

DrmPipeline::DrmPipeline(void *pageflipUserData, DrmGpu *gpu, DrmConnector *conn, DrmCrtc *crtc, DrmPlane *primaryPlane, DrmPlane *cursorPlane)
    : m_pageflipUserData(pageflipUserData), m_gpu(gpu), m_connector(conn), m_crtc(crtc), m_primaryPlane(primaryPlane)
{
    m_cursor.plane = cursorPlane;
    const auto &mode = m_connector->currentMode();
    m_mode.mode = mode.mode;
    m_mode.sourceSize = mode.size;
    m_mode.enabled = true;
    m_mode.blobId = -1;// = keep mode
}

DrmPipeline::~DrmPipeline()
{
    if (m_mode.blobId > 0) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_mode.blobId);
    }
    if (m_gamma.blobId > 0) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_gamma.blobId);
    }
}

bool DrmPipeline::present(const QSharedPointer<DrmBuffer> &buffer)
{
    if (m_gpu->useEglStreams() && !m_mode.changed && !m_gamma.changed) {
        // EglStreamBackend queues normal page flips through EGL,
        // modesets etc are performed through DRM-KMS
        return true;
    }
    setPrimaryBuffer(buffer);
    bool result = m_gpu->atomicModeSetting() ? atomicCommit(false) : presentLegacy();
    if (result) {
        m_mode.changed = false;
        m_gamma.changed = false;
    } else {
        qCWarning(KWIN_DRM) << "Present failed!" << strerror(errno);
    }
    return result;
}

bool DrmPipeline::test()
{
    if (m_gpu->atomicModeSetting()) {
        return atomicCommit(true);
    } else {
        return true;
    }
}

bool DrmPipeline::atomicCommit(bool testOnly)
{
    uint32_t flags = 0;
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
        qCDebug(KWIN_DRM) << "Failed to allocate drmModeAtomicReq!" << strerror(errno);
        return false;
    }
    if (testOnly) {
        checkTestBuffer();
    }
    m_primaryPlane->setNext(testOnly ? m_testBuffer : m_primaryBuffer);
    for (int i = 0; i < m_overlayPlanes.count(); i++) {
        m_overlayPlanes[i]->setNext(testOnly ? m_testBuffer : m_overlayBuffers[i]);
    }
    bool result = populateAtomicValues(req, flags);
    if (result && drmModeAtomicCommit(m_gpu->fd(), req, (flags & (~DRM_MODE_PAGE_FLIP_EVENT)) | DRM_MODE_ATOMIC_TEST_ONLY, m_pageflipUserData)) {
        result = false;
    }
    if (!testOnly && result && drmModeAtomicCommit(m_gpu->fd(), req, flags, m_pageflipUserData)) {
        qCCritical(KWIN_DRM) << "Atomic commit failed! This never should've happened!" << strerror(errno);
        result = false;
    }
    drmModeAtomicFree(req);
    return result;
}

bool DrmPipeline::populateAtomicValues(drmModeAtomicReq* req, uint32_t &flags)
{
    if (!m_gpu->useEglStreams() && m_mode.enabled) {
        flags |= DRM_MODE_PAGE_FLIP_EVENT;
    }
    if (m_mode.changed) {
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    } else {
        flags |= DRM_MODE_ATOMIC_NONBLOCK;
    }

    m_connector->setValue(DrmConnector::PropertyIndex::CrtcId, m_mode.enabled ? m_crtc->id() : 0);
    if (!m_connector->atomicPopulate(req)) {
        qCWarning(KWIN_DRM) << "Atomic populate for connector failed!";
        return false;
    }

    m_crtc->setValue(DrmCrtc::PropertyIndex::ModeId, m_mode.enabled ? m_mode.blobId : 0);
    m_crtc->setValue(DrmCrtc::PropertyIndex::Active, m_mode.enabled ? 1 : 0);
    m_crtc->setValue(DrmCrtc::PropertyIndex::Gamma_LUT, m_mode.enabled ? m_gamma.blobId : 0);
    if (!m_crtc->atomicPopulate(req)) {
        qCWarning(KWIN_DRM) << "Atomic populate for crtc failed!";
        return false;
    }

    QSize modesize = QSize(m_mode.mode.hdisplay, m_mode.mode.vdisplay);
    m_primaryPlane->setScaled(m_primaryPlane->next() ? m_primaryPlane->next()->size() : m_mode.sourceSize, modesize, m_crtc->id(), m_mode.enabled);
    m_primaryPlane->setTransformation(m_mode.transformation);
    if (!m_primaryPlane->atomicPopulate(req)) {
        qCWarning(KWIN_DRM) << "Atomic populate for primary plane failed!";
        return false;
    }

    if (m_cursor.plane) {
        const QSize &size = m_cursor.buffer ? m_cursor.buffer->size() : QSize(0, 0);
        m_cursor.plane->setNext(m_cursor.buffer);
        m_cursor.plane->set(size, m_cursor.pos, size, m_crtc->id(), m_mode.enabled && m_cursor.buffer != nullptr);
        m_cursor.plane->setTransformation(m_mode.transformation);
        if (!m_cursor.plane->atomicPopulate(req)) {
            qCWarning(KWIN_DRM) << "Atomic populate for cursor plane failed!";
            return false;
        }
    }

    for (const auto &overlay : qAsConst(m_overlayPlanes)) {
        overlay->setScaled(overlay->next() ? overlay->next()->size() : m_mode.sourceSize, modesize, m_crtc->id(), m_mode.enabled);
        overlay->setTransformation(m_mode.transformation);
        if (!overlay->atomicPopulate(req)) {
            qCWarning(KWIN_DRM) << "Atomic populate for overlay plane failed!";
            return false;
        }
    }

    return true;
}

bool DrmPipeline::presentLegacy()
{
    uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT;
    m_crtc->setNext(m_primaryBuffer);
    if (drmModePageFlip(m_gpu->fd(), m_crtc->id(), m_primaryBuffer ? m_primaryBuffer->bufferId() : 0, flags, m_pageflipUserData)) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno) << m_primaryBuffer;
        return false;
    }
    return true;
}

bool DrmPipeline::addOverlayPlane(DrmPlane *plane)
{
    if (m_gpu->atomicModeSetting()) {
        m_overlayPlanes << plane;
        if (!atomicCommit(true)) {
            qCWarning(KWIN_DRM) << "Could not add overlay plane!";
            m_overlayPlanes.removeOne(plane);
            return false;
        }
        m_overlayBuffers << nullptr;
    } else {
        return false;
    }
    return true;
}

void DrmPipeline::setPrimaryBuffer(const QSharedPointer<DrmBuffer> &buffer)
{
    m_primaryBuffer = buffer;
}

bool DrmPipeline::modeset(QSize source, drmModeModeInfo mode)
{
    auto oldMode = m_mode;
    m_mode.sourceSize = source;
    m_mode.mode = mode;
    if (m_gpu->atomicModeSetting()) {
        m_mode.changed = true;
        if (drmModeCreatePropertyBlob(m_gpu->fd(), &mode, sizeof(drmModeModeInfo), &m_mode.blobId)) {
            qCWarning(KWIN_DRM) << "Failed to create property blob for mode!" << strerror(errno);
            m_mode = oldMode;
            return false;
        }
        if (!atomicCommit(true)) {
            drmModeDestroyPropertyBlob(m_gpu->fd(), m_mode.blobId);
            m_mode = oldMode;
            return false;
        }
        if (oldMode.blobId) {
            drmModeDestroyPropertyBlob(m_gpu->fd(), oldMode.blobId);
        }
    } else {
        checkTestBuffer();
        uint32_t connId = m_connector->id();
        if (drmModeSetCrtc(m_gpu->fd(), m_crtc->id(), m_testBuffer->bufferId(), 0, 0, &connId, 1, &mode)) {
            m_mode = oldMode;
            qCWarning(KWIN_DRM) << "Modeset failed:" << strerror(errno);
            return false;
        }
    }
    return true;
}

bool DrmPipeline::setCursor(const QSharedPointer<DrmDumbBuffer> &buffer)
{
    auto oldCursor = m_cursor;
    m_cursor.buffer = buffer;
    if (m_gpu->atomicModeSetting() && m_cursor.plane) {
        if (!atomicCommit(true)) {
            qCWarning(KWIN_DRM) << "Could not set cursor!";
            m_cursor = oldCursor;
            return false;
        }
    } else {
        const QSize &s = buffer ? buffer->size() : QSize(0, 0);
        if (drmModeSetCursor(m_gpu->fd(), m_crtc->id(), m_cursor.buffer ? m_cursor.buffer->handle() : 0, s.width(), s.height())) {
            m_cursor = oldCursor;
            qCWarning(KWIN_DRM) << "Could not set cursor:" << strerror(errno);
            return false;
        }
    }
    return true;
}

bool DrmPipeline::moveCursor(QPoint pos)
{
    auto cursor = m_cursor;
    m_cursor.pos = pos;
    if (m_gpu->atomicModeSetting() && m_cursor.plane) {
        if (!atomicCommit(true)) {
            m_cursor = cursor;
            return false;
        }
    } else {
        if (drmModeMoveCursor(m_gpu->fd(), m_crtc->id(), m_cursor.pos.x(), m_cursor.pos.y())) {
            m_cursor = cursor;
            return false;
        }
    }
    return true;
}

bool DrmPipeline::setEnablement(bool enabled)
{
    auto oldMode = m_mode;
    m_mode.enabled = enabled;
    setPrimaryBuffer(nullptr);
    if (m_gpu->atomicModeSetting()) {
        m_mode.changed = true;
        // immediately commit if disabling as there will be no present
        if (!atomicCommit(enabled)) {
            m_mode = oldMode;
            return false;
        }
    } else {
        if (!m_connector->dpms()) {
            qCWarning(KWIN_DRM) << "Setting DPMS failed: dpms property missing!";
            m_mode = oldMode;
            return false;
        }
        if (drmModeConnectorSetProperty(m_gpu->fd(), m_connector->id(), m_connector->dpms()->propId(), m_mode.enabled ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF) < 0) {
            qCWarning(KWIN_DRM) << "Setting DPMS failed";
            m_mode = oldMode;
            return false;
        }
    }
    return true;
}

bool DrmPipeline::setGammaRamp(const GammaRamp &ramp)
{
    // Apparently there are old Intel iGPUs that don't have full support
    // for setting the gamma ramp with AMS -> fall back to legacy in that case
    if (m_gpu->atomicModeSetting() && m_crtc->hasGammaProp()) {
        auto oldGamma = m_gamma;
        m_gamma.changed = true;

        struct drm_color_lut *gamma = new drm_color_lut[ramp.size()];
        for (uint32_t i = 0; i < ramp.size(); i++) {
            gamma[i].red = ramp.red()[i];
            gamma[i].green = ramp.green()[i];
            gamma[i].blue = ramp.blue()[i];
        }
        bool result = !drmModeCreatePropertyBlob(m_gpu->fd(), gamma, ramp.size() * sizeof(struct drm_color_lut), &m_gamma.blobId);
        delete[] gamma;
        if (!result) {
            qCWarning(KWIN_DRM) << "Could not create gamma LUT property blob" << strerror(errno);
            m_gamma = oldGamma;
            return false;
        }
        if (!atomicCommit(true)) {
            qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
            drmModeDestroyPropertyBlob(m_gpu->fd(), m_gamma.blobId);
            m_gamma = oldGamma;
            return false;
        }
        if (oldGamma.blobId) {
            drmModeDestroyPropertyBlob(m_gpu->fd(), oldGamma.blobId);
        }
    } else {
        uint16_t *red = const_cast<uint16_t*>(ramp.red());
        uint16_t *green = const_cast<uint16_t*>(ramp.green());
        uint16_t *blue = const_cast<uint16_t*>(ramp.blue());
        bool result = !drmModeCrtcSetGamma(m_gpu->fd(), m_crtc->id(), ramp.size(), red, green, blue);
        if (!result) {
            qCWarning(KWIN_DRM) << "setting gamma failed!" << strerror(errno);
            return false;
        }
    }
    return true;
}

bool DrmPipeline::setTransformation(const QSize &srcSize, const DrmPlane::Transformations &transformation)
{
    if (m_gpu->atomicModeSetting()) {
        const auto oldMode = m_mode;
        m_mode.transformation = transformation;
        m_mode.sourceSize = srcSize;
        m_mode.changed = true;
        if (!atomicCommit(true)) {
            m_mode = oldMode;
            return false;
        }
    } else {
        return false;
    }
    return true;
}

void DrmPipeline::checkTestBuffer()
{
    if (m_testBuffer && m_testBuffer->size() == m_mode.sourceSize) {
        return;
    }
    m_testBuffer = m_gpu->createTestbuffer(m_mode.sourceSize);
}

DrmPlane::Transformations DrmPipeline::transformation() const
{
    return m_mode.transformation;
}

}
