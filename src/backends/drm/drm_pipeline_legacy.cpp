/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_pipeline.h"
#include "logging.h"

#include <errno.h>
#include <gbm.h>

namespace KWin
{

bool DrmPipeline::presentLegacy()
{
    if (!m_pending.crtc->current() && !legacyModeset()) {
        return false;
    }
    const auto buffer = m_pending.layer->currentBuffer();
    if (drmModePageFlip(gpu()->fd(), m_pending.crtc->id(), buffer->framebufferId(), DRM_MODE_PAGE_FLIP_EVENT, nullptr) != 0) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno);
        return false;
    }
    m_pageflipPending = true;
    m_pending.crtc->setNext(buffer);
    return true;
}

bool DrmPipeline::legacyModeset()
{
    uint32_t connId = m_connector->id();
    if (!m_pending.layer->checkTestBuffer()) {
        return false;
    }
    const auto buffer = m_pending.layer->currentBuffer();
    if (drmModeSetCrtc(gpu()->fd(), m_pending.crtc->id(), buffer->framebufferId(), 0, 0, &connId, 1, m_pending.mode->nativeMode()) != 0) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        return false;
    }
    // make sure the buffer gets kept alive, or the modeset gets reverted by the kernel
    if (m_pending.crtc->current()) {
        m_pending.crtc->setNext(buffer);
    } else {
        m_pending.crtc->setCurrent(buffer);
    }
    return true;
}

bool DrmPipeline::commitPipelinesLegacy(const QVector<DrmPipeline *> &pipelines, CommitMode mode)
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
            pipeline->m_current = pipeline->m_pending;
            if (mode == CommitMode::CommitModeset && mode != CommitMode::Test && pipeline->activePending()) {
                pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
            }
        }
        return true;
    }
}

bool DrmPipeline::applyPendingChangesLegacy()
{
    if (!m_pending.active && m_pending.crtc) {
        drmModeSetCursor(gpu()->fd(), m_pending.crtc->id(), 0, 0, 0);
    }
    if (activePending()) {
        auto vrr = m_pending.crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled);
        if (vrr && !vrr->setPropertyLegacy(m_pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return false;
        }
        if (const auto &rgbRange = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
            rgbRange->setEnumLegacy(m_pending.rgbRange);
        }
        if (const auto overscan = m_connector->getProp(DrmConnector::PropertyIndex::Overscan)) {
            overscan->setPropertyLegacy(m_pending.overscan);
        } else if (const auto underscan = m_connector->getProp(DrmConnector::PropertyIndex::Underscan)) {
            const uint32_t hborder = calculateUnderscan();
            underscan->setEnumLegacy(m_pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
            m_connector->getProp(DrmConnector::PropertyIndex::Underscan_vborder)->setPropertyLegacy(m_pending.overscan);
            m_connector->getProp(DrmConnector::PropertyIndex::Underscan_hborder)->setPropertyLegacy(hborder);
        }
        if (needsModeset() && !legacyModeset()) {
            return false;
        }
        if (m_pending.gamma && drmModeCrtcSetGamma(gpu()->fd(), m_pending.crtc->id(), m_pending.gamma->lut().size(), m_pending.gamma->lut().red(), m_pending.gamma->lut().green(), m_pending.gamma->lut().blue()) != 0) {
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
    const auto bo = cursorLayer()->currentBuffer();
    const uint32_t handle = bo && bo->buffer() && cursorLayer()->isVisible() ? bo->buffer()->handles()[0] : 0;

    struct drm_mode_cursor2 arg = {
        .flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE,
        .crtc_id = m_pending.crtc->id(),
        .x = m_pending.cursorLayer->position().x(),
        .y = m_pending.cursorLayer->position().y(),
        .width = (uint32_t)gpu()->cursorSize().width(),
        .height = (uint32_t)gpu()->cursorSize().height(),
        .handle = handle,
        .hot_x = m_pending.cursorHotspot.x(),
        .hot_y = m_pending.cursorHotspot.y(),
    };
    return drmIoctl(gpu()->fd(), DRM_IOCTL_MODE_CURSOR2, &arg) == 0;
}

bool DrmPipeline::moveCursorLegacy()
{
    return drmModeMoveCursor(gpu()->fd(), m_pending.crtc->id(), cursorLayer()->position().x(), cursorLayer()->position().y()) == 0;
}

}
