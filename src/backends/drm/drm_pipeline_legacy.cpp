/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "drm_pipeline.h"
#include "drm_gpu.h"
#include "drm_buffer.h"
#include "drm_object_crtc.h"
#include "drm_object_connector.h"
#include "logging.h"

namespace KWin
{

bool DrmPipeline::presentLegacy()
{
    if ((!pending.crtc->current() || pending.crtc->current()->needsModeChange(m_primaryBuffer.get())) && !legacyModeset()) {
        return false;
    }
    QVector<DrmPipeline*> *userData = new QVector<DrmPipeline*>();
    *userData << this;
    if (drmModePageFlip(gpu()->fd(), pending.crtc->id(), m_primaryBuffer ? m_primaryBuffer->bufferId() : 0, DRM_MODE_PAGE_FLIP_EVENT, userData) != 0) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno) << m_primaryBuffer;
        return false;
    }
    m_pageflipPending = true;
    pending.crtc->setNext(m_primaryBuffer);
    return true;
}

bool DrmPipeline::legacyModeset()
{
    auto mode = m_connector->modes()[pending.modeIndex];
    uint32_t connId = m_connector->id();
    if (!checkTestBuffer() || drmModeSetCrtc(gpu()->fd(), pending.crtc->id(), m_primaryBuffer->bufferId(), 0, 0, &connId, 1, mode->nativeMode()) != 0) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        pending = m_next;
        m_primaryBuffer = m_oldTestBuffer;
        return false;
    }
    m_oldTestBuffer = nullptr;
    // make sure the buffer gets kept alive, or the modeset gets reverted by the kernel
    if (pending.crtc->current()) {
        pending.crtc->setNext(m_primaryBuffer);
    } else {
        pending.crtc->setCurrent(m_primaryBuffer);
    }
    return true;
}

bool DrmPipeline::commitPipelinesLegacy(const QVector<DrmPipeline*> &pipelines, CommitMode mode)
{
    bool failure = false;
    for (const auto &pipeline : pipelines) {
        if (!pipeline->applyPendingChangesLegacy()) {
            failure = true;
            break;
        }
    }
    if (failure) {
        // at least try to revert the config
        for (const auto &pipeline : pipelines) {
            pipeline->revertPendingChanges();
            pipeline->applyPendingChangesLegacy();
        }
        return false;
    } else {
        for (const auto &pipeline : pipelines) {
            pipeline->applyPendingChanges();
            pipeline->m_current = pipeline->pending;
            if (mode == CommitMode::CommitModeset && mode != CommitMode::Test && pipeline->activePending()) {
                pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
            }
        }
        return true;
    }
}

bool DrmPipeline::applyPendingChangesLegacy()
{
    if (!pending.active && pending.crtc) {
        drmModeSetCursor(gpu()->fd(), pending.crtc->id(), 0, 0, 0);
    }
    if (pending.active) {
        Q_ASSERT(pending.crtc);
        auto vrr = pending.crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled);
        if (vrr && !vrr->setPropertyLegacy(pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return false;
        }
        if (const auto &rgbRange = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
            rgbRange->setEnumLegacy(pending.rgbRange);
        }
        if (needsModeset() &&!legacyModeset()) {
            return false;
        }
        m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setCurrent(DRM_MODE_DPMS_ON);
        if (pending.gamma && drmModeCrtcSetGamma(gpu()->fd(), pending.crtc->id(), pending.gamma->size(),
                                    pending.gamma->red(), pending.gamma->green(), pending.gamma->blue()) != 0) {
            qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
            return false;
        }
        pending.crtc->setLegacyCursor();
    }
    if (!m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setPropertyLegacy(pending.active ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return false;
    }
    return true;
}

}
