/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "drm_buffer.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_pipeline.h"

#include <errno.h>
#include <gbm.h>

namespace KWin
{

DrmPipeline::Error DrmPipeline::presentLegacy()
{
    if (!m_pending.crtc->current()) {
        Error err = legacyModeset();
        if (err != Error::None) {
            return err;
        }
    }
    const auto buffer = m_pending.layer->currentBuffer();
    uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT;
    if (m_pending.syncMode == RenderLoopPrivate::SyncMode::Async || m_pending.syncMode == RenderLoopPrivate::SyncMode::AdaptiveAsync) {
        flags |= DRM_MODE_PAGE_FLIP_ASYNC;
    }
    if (drmModePageFlip(gpu()->fd(), m_pending.crtc->id(), buffer->framebufferId(), flags, gpu()) != 0) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno);
        return errnoToError();
    }
    m_pageflipPending = true;
    m_pending.crtc->setNext(buffer);
    return Error::None;
}

DrmPipeline::Error DrmPipeline::legacyModeset()
{
    uint32_t connId = m_connector->id();
    if (!m_pending.layer->checkTestBuffer()) {
        return Error::TestBufferFailed;
    }
    const auto buffer = m_pending.layer->currentBuffer();
    if (drmModeSetCrtc(gpu()->fd(), m_pending.crtc->id(), buffer->framebufferId(), 0, 0, &connId, 1, m_pending.mode->nativeMode()) != 0) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        return errnoToError();
    }
    // make sure the buffer gets kept alive, or the modeset gets reverted by the kernel
    if (m_pending.crtc->current()) {
        m_pending.crtc->setNext(buffer);
    } else {
        m_pending.crtc->setCurrent(buffer);
    }
    return Error::None;
}

DrmPipeline::Error DrmPipeline::commitPipelinesLegacy(const QVector<DrmPipeline *> &pipelines, CommitMode mode)
{
    Error err = Error::None;
    for (const auto &pipeline : pipelines) {
        err = pipeline->applyPendingChangesLegacy();
        if (err != Error::None) {
            break;
        }
    }
    if (err != Error::None) {
        // at least try to revert the config
        for (const auto &pipeline : pipelines) {
            pipeline->revertPendingChanges();
            pipeline->applyPendingChangesLegacy();
        }
    } else {
        for (const auto &pipeline : pipelines) {
            pipeline->applyPendingChanges();
            pipeline->m_current = pipeline->m_pending;
            if (mode == CommitMode::CommitModeset && pipeline->activePending()) {
                pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
            }
        }
    }
    return err;
}

DrmPipeline::Error DrmPipeline::applyPendingChangesLegacy()
{
    if (!m_pending.active && m_pending.crtc) {
        drmModeSetCursor(gpu()->fd(), m_pending.crtc->id(), 0, 0, 0);
    }
    if (activePending()) {
        auto vrr = m_pending.crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled);
        if (vrr && !vrr->setPropertyLegacy(m_pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive || m_pending.syncMode == RenderLoopPrivate::SyncMode::AdaptiveAsync)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return errnoToError();
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
        if (const auto scaling = m_connector->getProp(DrmConnector::PropertyIndex::ScalingMode); scaling && scaling->hasEnum(DrmConnector::ScalingMode::None)) {
            scaling->setEnumLegacy(DrmConnector::ScalingMode::None);
        }
        if (m_pending.crtc != m_current.crtc || m_pending.mode != m_current.mode) {
            Error err = legacyModeset();
            if (err != Error::None) {
                return err;
            }
        }
        if (m_pending.gamma && drmModeCrtcSetGamma(gpu()->fd(), m_pending.crtc->id(), m_pending.gamma->lut().size(), m_pending.gamma->lut().red(), m_pending.gamma->lut().green(), m_pending.gamma->lut().blue()) != 0) {
            qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
            return errnoToError();
        }
        if (const auto contentType = m_connector->getProp(DrmConnector::PropertyIndex::ContentType)) {
            contentType->setEnumLegacy(m_pending.contentType);
        }
        setCursorLegacy();
        moveCursorLegacy();
    }
    if (!m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setPropertyLegacy(activePending() ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return errnoToError();
    }
    return Error::None;
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
