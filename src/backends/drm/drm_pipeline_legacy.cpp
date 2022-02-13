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
#include "drm_layer.h"

#include <errno.h>

namespace KWin
{

bool DrmPipeline::presentLegacy()
{
    const auto buffer = pending.layer->currentBuffer();
    if ((!pending.crtc->current() || pending.crtc->current()->needsModeChange(buffer.get())) && !legacyModeset()) {
        return false;
    }
    if (drmModePageFlip(gpu()->fd(), pending.crtc->id(), buffer ? buffer->bufferId() : 0, DRM_MODE_PAGE_FLIP_EVENT, nullptr) != 0) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno) << buffer;
        return false;
    }
    m_pageflipPending = true;
    pending.crtc->setNext(buffer);
    return true;
}

bool DrmPipeline::legacyModeset()
{
    uint32_t connId = m_connector->id();
    if (!pending.layer->testBuffer() || drmModeSetCrtc(gpu()->fd(), pending.crtc->id(), pending.layer->currentBuffer()->bufferId(), 0, 0, &connId, 1, pending.mode->nativeMode()) != 0) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        pending = m_next;
        return false;
    }
    // make sure the buffer gets kept alive, or the modeset gets reverted by the kernel
    if (pending.crtc->current()) {
        pending.crtc->setNext(pending.layer->currentBuffer());
    } else {
        pending.crtc->setCurrent(pending.layer->currentBuffer());
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
    if (activePending()) {
        auto vrr = pending.crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled);
        if (vrr && !vrr->setPropertyLegacy(pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return false;
        }
        if (const auto &rgbRange = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
            rgbRange->setEnumLegacy(pending.rgbRange);
        }
        if (const auto overscan = m_connector->getProp(DrmConnector::PropertyIndex::Overscan)) {
            overscan->setPropertyLegacy(pending.overscan);
        } else if (const auto underscan = m_connector->getProp(DrmConnector::PropertyIndex::Underscan)) {
            const uint32_t hborder = calculateUnderscan();
            underscan->setEnumLegacy(pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
            m_connector->getProp(DrmConnector::PropertyIndex::Underscan_vborder)->setPropertyLegacy(pending.overscan);
            m_connector->getProp(DrmConnector::PropertyIndex::Underscan_hborder)->setPropertyLegacy(hborder);
        }
        if (needsModeset() &&!legacyModeset()) {
            return false;
        }
        if (pending.gamma && drmModeCrtcSetGamma(gpu()->fd(), pending.crtc->id(), pending.gamma->size(),
                                    pending.gamma->red(), pending.gamma->green(), pending.gamma->blue()) != 0) {
            qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
            return false;
        }
        setCursorLegacy();
        moveCursorLegacy();
    }
    if (!m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setPropertyLegacy(activePending() ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return false;
    }
    return true;
}

bool DrmPipeline::setCursorLegacy()
{
    const QSize &s = pending.cursorBo ? pending.cursorBo->size() : QSize(64, 64);
    int ret = drmModeSetCursor2(gpu()->fd(), pending.crtc->id(),
                                pending.cursorBo ? pending.cursorBo->handle() : 0,
                                s.width(), s.height(),
                                pending.cursorHotspot.x(), pending.cursorHotspot.y());
    if (ret == -ENOTSUP) {
        // for NVIDIA case that does not support drmModeSetCursor2
        ret = drmModeSetCursor(gpu()->fd(), pending.crtc->id(),
                               pending.cursorBo ? pending.cursorBo->handle() : 0,
                               s.width(), s.height());
    }
    return ret == 0;
}

bool DrmPipeline::moveCursorLegacy()
{
    return drmModeMoveCursor(gpu()->fd(), pending.crtc->id(), pending.cursorPos.x(), pending.cursorPos.y()) == 0;
}

}
