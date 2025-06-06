/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/renderbackend.h"
#include "drm_layer.h"
#include "utils/damagejournal.h"

#include <QImage>

namespace KWin
{

class QPainterSwapchain;
class QPainterSwapchainSlot;
class DrmPipeline;
class DrmVirtualOutput;
class DrmQPainterBackend;
class DrmFramebuffer;

class DrmQPainterLayer : public DrmPipelineLayer
{
public:
    explicit DrmQPainterLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    bool preparePresentationTest() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    void releaseBuffers() override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    QList<QSize> recommendedSizes() const override;
    ColorDescription colorDescription() const override;

private:
    bool doesSwapchainFit() const;

    std::shared_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_currentBuffer;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
    DamageJournal m_damageJournal;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};

class DrmVirtualQPainterLayer : public DrmOutputLayer
{
public:
    explicit DrmVirtualQPainterLayer(DrmVirtualOutput *output);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    void releaseBuffers() override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    QImage m_image;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};
}
