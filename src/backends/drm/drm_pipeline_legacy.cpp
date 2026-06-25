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
#include "drm_output.h"
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

std::expected<void, OutputError> DrmPipeline::presentLegacy(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    auto ret = applyPendingChangesLegacy();
    if (!ret) {
        return ret;
    }
    if (auto cursor = findLayer(layersToUpdate, OutputLayerType::CursorOnly)) {
        ret = setCursorLegacy(cursor);
        if (!ret) {
            return ret;
        }
    }
    // always present on the crtc, for presentation feedback
    const auto primary = findLayer(m_pending.layers, OutputLayerType::Primary);
    const auto buffer = primary->currentBuffer();
    if (primary->sourceRect() != primary->targetRect() || primary->targetRect() != Rect(QPoint(0, 0), buffer->buffer()->size())) {
        return std::unexpected(OutputError{
            .code = OutputErrorCode::Scaling,
            .message = QStringLiteral("Legacy modesetting can't support scaling or cropping"),
        });
    }
    auto commit = std::make_unique<DrmLegacyCommit>(this, buffer, frame);
    if (!commit->doPageflip(m_pending.presentationMode)) {
        qCDebug(KWIN_DRM) << "Page flip failed:" << strerror(errno);
        return errnoToError();
    }
    m_commitThread->setPendingCommit(std::move(commit));
    return {};
}

void DrmPipeline::forceLegacyModeset()
{
    if (activePending()) {
        (void)legacyModeset();
        (void)setLegacyGamma();
    }
}

std::expected<void, OutputError> DrmPipeline::legacyModeset()
{
    const auto primary = findLayer(m_pending.layers, OutputLayerType::Primary);
    const auto buffer = primary->currentBuffer();
    if (!buffer) {
        return std::unexpected(OutputError{
            .code = OutputErrorCode::InvalidApiUsage,
            .message = QStringLiteral("Can't modeset without a buffer"),
        });
    }
    if (primary->sourceRect() != RectF(QPoint(0, 0), buffer->buffer()->size())) {
        return std::unexpected(OutputError{
            .code = OutputErrorCode::Scaling,
            .message = QStringLiteral("Legacy modesetting can't support scaling or cropping"),
        });
    }
    auto commit = std::make_unique<DrmLegacyCommit>(this, buffer, nullptr);
    if (!commit->doModeset(m_connector, m_pending.mode.get())) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        return errnoToError();
    }
    return {};
}

std::expected<void, OutputError> DrmPipeline::commitPipelinesLegacy(const QList<DrmPipeline *> &pipelines, DrmGpu *gpu, CommitMode mode, const QList<DrmObject *> &unusedObjects)
{
    std::expected<void, OutputError> ret;
    for (DrmPipeline *pipeline : pipelines) {
        ret = pipeline->applyPendingChangesLegacy();
        if (!ret) {
            break;
        }
    }
    if (!ret) {
        // at least try to revert the config
        for (DrmPipeline *pipeline : pipelines) {
            pipeline->revertPendingChanges();
            (void)pipeline->applyPendingChangesLegacy();
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
                drmModeSetCrtc(gpu->fd(), crtc->id(), 0, 0, 0, nullptr, 0, nullptr);
            }
        }
    }
    return ret;
}

std::expected<void, OutputError> DrmPipeline::applyPendingChangesLegacy()
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
            return std::unexpected(OutputError{
                .code = OutputErrorCode::ColorPipeline,
                .message = QStringLiteral("Legacy modesetting can't sync color changes to pageflips"),
            });
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
            return std::unexpected(OutputError{
                .code = OutputErrorCode::InvalidApiUsage,
                .message = QStringLiteral("Can't enable HDR without driver support"),
            });
        }
        if (m_connector->colorspace.isValid()) {
            m_connector->colorspace.setEnumLegacy(m_pending.wcg ? DrmConnector::Colorspace::BT2020_RGB : DrmConnector::Colorspace::Default);
        } else if (m_pending.wcg) {
            return std::unexpected(OutputError{
                .code = OutputErrorCode::InvalidApiUsage,
                .message = QStringLiteral("Can't enable WCG without driver support"),
            });
        }
        const auto currentModeContent = m_pending.crtc->queryCurrentMode();
        if (m_pending.crtc != m_next.crtc || *m_pending.mode != currentModeContent) {
            qCDebug(KWIN_DRM) << "Using legacy path to set mode" << m_pending.mode->nativeMode()->name;
            auto ret = legacyModeset();
            if (!ret) {
                return ret;
            }
        }
        if (m_pending.crtcColorPipeline != m_currentLegacyGamma) {
            auto ret = setLegacyGamma();
            if (!ret) {
                return ret;
            }
        }
        if (m_connector->contentType.isValid()) {
            m_connector->contentType.setEnumLegacy(m_pending.contentType);
        }
        if (m_connector->maxBpc.isValid()) {
            m_connector->maxBpc.setPropertyLegacy(8);
        }
        auto ret = setCursorLegacy(findLayer(m_pending.layers, OutputLayerType::CursorOnly));
        if (!ret) {
            return ret;
        }
    }
    if (!m_connector->dpms.setPropertyLegacy(activePending() ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return errnoToError();
    }
    return {};
}

std::expected<void, OutputError> DrmPipeline::setLegacyGamma()
{
    QList<uint16_t> red(m_pending.crtc->gammaRampSize());
    QList<uint16_t> green(m_pending.crtc->gammaRampSize());
    QList<uint16_t> blue(m_pending.crtc->gammaRampSize());
    // some drivers with legacy modesetting implicitly apply the
    // gamma lut in a "linear" space, assuming that the signal sent
    // to the display is encoded with the piece-wise sRGB transfer function
    TransferFunction workaround(TransferFunction::linear, 0, 1);
    if (gpu()->drmDevice()->isRadeon() || gpu()->drmDevice()->isAmdgpu() || gpu()->drmDevice()->isNouveau()) {
        workaround = TransferFunction(TransferFunction::sRGB, 0, 1);
    }
    for (int i = 0; i < m_pending.crtc->gammaRampSize(); i++) {
        const double input = i / double(m_pending.crtc->gammaRampSize() - 1);
        QVector3D output = QVector3D(input, input, input);
        output = workaround.encodedToNits(output);
        for (const auto &op : m_pending.crtcColorPipeline.ops) {
            if (auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
                output = tf->tf.encodedToNits(output);
            } else if (auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
                output = tf->tf.nitsToEncoded(output);
            } else if (auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
                output *= mult->factors;
            } else {
                // not supported
                return std::unexpected(OutputError{
                    .code = OutputErrorCode::ColorPipeline,
                    .message = QStringLiteral("Legacy modesetting only supports LUT operations"),
                });
            }
        }
        output = workaround.nitsToEncoded(output);
        red[i] = std::clamp(output.x(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max();
        green[i] = std::clamp(output.y(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max();
        blue[i] = std::clamp(output.z(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max();
    }
    if (drmModeCrtcSetGamma(gpu()->fd(), m_pending.crtc->id(), m_pending.crtc->gammaRampSize(), red.data(), green.data(), blue.data()) != 0) {
        qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
        return errnoToError();
    }
    m_currentLegacyGamma = m_pending.crtcColorPipeline;
    return {};
}

std::expected<void, OutputError> DrmPipeline::setCursorLegacy(DrmPipelineLayer *layer)
{
    const auto bo = layer->currentBuffer();
    uint32_t handle = 0;
    if (bo && bo->buffer() && layer->isEnabled()) {
        const DmaBufAttributes *attributes = bo->buffer()->dmabufAttributes();
        if (drmPrimeFDToHandle(gpu()->fd(), attributes->fd[0].get(), &handle) != 0) {
            qCWarning(KWIN_DRM) << "drmPrimeFDToHandle() failed";
            return errnoToError();
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
    if (ret != 0) {
        return errnoToError();
    }
    return {};
}

}
