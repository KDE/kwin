/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "core/graphicsbuffer.h"
#include "drm_buffer.h"
#include "drm_commit.h"
#include "drm_commit_thread.h"
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

static DrmPipelineLayer *findLayer(const auto &layers, OutputLayerType type)
{
    const auto it = std::ranges::find_if(layers, [type](OutputLayer *layer) {
        return layer->type() == type;
    });
    return it == layers.end() ? nullptr : static_cast<DrmPipelineLayer *>(*it);
}

DrmPipeline::Error DrmPipeline::presentLegacy(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    if (Error err = applyPendingChangesLegacy(); err != Error::None) {
        return err;
    }
    if (auto cursor = findLayer(layersToUpdate, OutputLayerType::CursorOnly); cursor && !setCursorLegacy(cursor)) {
        return Error::InvalidArguments;
    }
    // always present on the crtc, for presentation feedback
    const auto primary = findLayer(m_pending.layers, OutputLayerType::Primary);
    const auto buffer = primary->currentBuffer();
    if (primary->sourceRect() != primary->targetRect() || primary->targetRect() != QRect(QPoint(0, 0), buffer->buffer()->size())) {
        return Error::InvalidArguments;
    }
    auto commit = std::make_unique<DrmLegacyCommit>(this, buffer, frame);
    if (!commit->doPageflip(m_pending.presentationMode)) {
        qCDebug(KWIN_DRM) << "Page flip failed:" << strerror(errno);
        return errnoToError();
    }
    m_commitThread->setPendingCommit(std::move(commit));
    return Error::None;
}

void DrmPipeline::forceLegacyModeset()
{
    if (activePending()) {
        legacyModeset();
        setLegacyGamma();
    }
}

DrmPipeline::Error DrmPipeline::legacyModeset()
{
    const auto primary = findLayer(m_pending.layers, OutputLayerType::Primary);
    const auto buffer = primary->currentBuffer();
    if (!buffer) {
        return Error::InvalidArguments;
    }
    if (primary->sourceRect() != QRect(QPoint(0, 0), buffer->buffer()->size())) {
        return Error::InvalidArguments;
    }
    auto commit = std::make_unique<DrmLegacyCommit>(this, buffer, nullptr);
    if (!commit->doModeset(m_connector, m_pending.mode.get())) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        return errnoToError();
    }
    return Error::None;
}

DrmPipeline::Error DrmPipeline::commitPipelinesLegacy(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects)
{
    Error err = Error::None;
    for (DrmPipeline *pipeline : pipelines) {
        err = pipeline->applyPendingChangesLegacy();
        if (err != Error::None) {
            break;
        }
    }
    if (err != Error::None) {
        // at least try to revert the config
        for (DrmPipeline *pipeline : pipelines) {
            pipeline->revertPendingChanges();
            pipeline->applyPendingChangesLegacy();
        }
    } else {
        for (DrmPipeline *pipeline : pipelines) {
            pipeline->applyPendingChanges();
            if (mode == CommitMode::CommitModeset && pipeline->activePending()) {
                pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
            }
        }
        for (DrmObject *obj : unusedObjects) {
            if (auto crtc = dynamic_cast<DrmCrtc *>(obj)) {
                drmModeSetCrtc(pipelines.front()->gpu()->fd(), crtc->id(), 0, 0, 0, nullptr, 0, nullptr);
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
        const bool colorTransforms = std::ranges::any_of(m_pending.layers, [](DrmPipelineLayer *layer) {
            return !layer->colorPipeline().isIdentity();
        });
        if (colorTransforms) {
            // while it's technically possible to set CRTC color management properties,
            // it may result in glitches
            return DrmPipeline::Error::InvalidArguments;
        }
        const bool shouldEnableVrr = m_pending.presentationMode == PresentationMode::AdaptiveSync || m_pending.presentationMode == PresentationMode::AdaptiveAsync;
        if (m_pending.crtc->vrrEnabled.isValid() && !m_pending.crtc->vrrEnabled.setPropertyLegacy(shouldEnableVrr)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return errnoToError();
        }
        if (m_connector->broadcastRGB.isValid()) {
            m_connector->broadcastRGB.setEnumLegacy(DrmConnector::rgbRangeToBroadcastRgb(m_pending.rgbRange));
        }
        if (m_connector->overscan.isValid()) {
            m_connector->overscan.setPropertyLegacy(m_pending.overscan);
        } else if (m_connector->underscan.isValid()) {
            const uint32_t hborder = calculateUnderscan();
            m_connector->underscan.setEnumLegacy(m_pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
            m_connector->underscanVBorder.setPropertyLegacy(m_pending.overscan);
            m_connector->underscanHBorder.setPropertyLegacy(hborder);
        }
        if (m_connector->scalingMode.isValid() && m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
            m_connector->scalingMode.setEnumLegacy(DrmConnector::ScalingMode::None);
        }
        if (m_connector->hdrMetadata.isValid()) {
            const auto blob = createHdrMetadata(m_pending.hdr ? TransferFunction::PerceptualQuantizer : TransferFunction::gamma22);
            m_connector->hdrMetadata.setPropertyLegacy(blob ? blob->blobId() : 0);
        } else if (m_pending.hdr) {
            return DrmPipeline::Error::InvalidArguments;
        }
        if (m_connector->colorspace.isValid()) {
            m_connector->colorspace.setEnumLegacy(m_pending.wcg ? DrmConnector::Colorspace::BT2020_RGB : DrmConnector::Colorspace::Default);
        } else if (m_pending.wcg) {
            return DrmPipeline::Error::InvalidArguments;
        }
        const auto currentModeContent = m_pending.crtc->queryCurrentMode();
        if (m_pending.crtc != m_next.crtc || *m_pending.mode != currentModeContent) {
            qCDebug(KWIN_DRM) << "Using legacy path to set mode" << m_pending.mode->nativeMode()->name;
            Error err = legacyModeset();
            if (err != Error::None) {
                return err;
            }
        }
        if (m_pending.crtcColorPipeline != m_currentLegacyGamma) {
            if (Error err = setLegacyGamma(); err != Error::None) {
                return err;
            }
        }
        if (m_connector->contentType.isValid()) {
            m_connector->contentType.setEnumLegacy(m_pending.contentType);
        }
        if (m_connector->maxBpc.isValid()) {
            m_connector->maxBpc.setPropertyLegacy(8);
        }
        setCursorLegacy(findLayer(m_pending.layers, OutputLayerType::CursorOnly));
    }
    if (!m_connector->dpms.setPropertyLegacy(activePending() ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return errnoToError();
    }
    return Error::None;
}

DrmPipeline::Error DrmPipeline::setLegacyGamma()
{
    QList<uint16_t> red(m_pending.crtc->gammaRampSize());
    QList<uint16_t> green(m_pending.crtc->gammaRampSize());
    QList<uint16_t> blue(m_pending.crtc->gammaRampSize());
    for (int i = 0; i < m_pending.crtc->gammaRampSize(); i++) {
        const double input = i / double(m_pending.crtc->gammaRampSize() - 1);
        QVector3D output = QVector3D(input, input, input);
        for (const auto &op : m_pending.crtcColorPipeline.ops) {
            if (auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
                output = tf->tf.encodedToNits(output);
            } else if (auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
                output = tf->tf.nitsToEncoded(output);
            } else if (auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
                output *= mult->factors;
            } else {
                // not supported
                return Error::InvalidArguments;
            }
        }
        red[i] = std::clamp(output.x(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max();
        green[i] = std::clamp(output.y(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max();
        blue[i] = std::clamp(output.z(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max();
    }
    if (drmModeCrtcSetGamma(gpu()->fd(), m_pending.crtc->id(), m_pending.crtc->gammaRampSize(), red.data(), green.data(), blue.data()) != 0) {
        qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
        return errnoToError();
    }
    m_currentLegacyGamma = m_pending.crtcColorPipeline;
    return DrmPipeline::Error::None;
}

bool DrmPipeline::setCursorLegacy(DrmPipelineLayer *layer)
{
    const auto bo = layer->currentBuffer();
    uint32_t handle = 0;
    if (bo && bo->buffer() && layer->isEnabled()) {
        const DmaBufAttributes *attributes = bo->buffer()->dmabufAttributes();
        if (drmPrimeFDToHandle(gpu()->fd(), attributes->fd[0].get(), &handle) != 0) {
            qCWarning(KWIN_DRM) << "drmPrimeFDToHandle() failed";
            return false;
        }
    }

    struct drm_mode_cursor2 arg = {
        .flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE,
        .crtc_id = m_pending.crtc->id(),
        .x = int32_t(layer->targetRect().x()),
        .y = int32_t(layer->targetRect().y()),
        .width = (uint32_t)gpu()->cursorSize().width(),
        .height = (uint32_t)gpu()->cursorSize().height(),
        .handle = handle,
        .hot_x = int32_t(layer->hotspot().x()),
        .hot_y = int32_t(layer->hotspot().y()),
    };
    const int ret = drmIoctl(gpu()->fd(), DRM_IOCTL_MODE_CURSOR2, &arg);

    if (handle != 0) {
        drmCloseBufferHandle(gpu()->fd(), handle);
    }
    return ret == 0;
}
}
