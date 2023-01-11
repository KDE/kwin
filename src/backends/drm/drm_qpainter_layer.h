/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include <QImage>

namespace KWin
{

class DumbSwapchain;
class DrmPipeline;
class DrmVirtualOutput;
class DrmQPainterBackend;
class DrmDumbBuffer;
class DrmFramebuffer;

class DrmQPainterLayer : public DrmPipelineLayer
{
public:
    DrmQPainterLayer(DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    void releaseBuffers() override;

private:
    bool doesSwapchainFit() const;

    std::shared_ptr<DumbSwapchain> m_swapchain;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
    QRegion m_currentDamage;
};

class DrmCursorQPainterLayer : public DrmOverlayLayer
{
public:
    DrmCursorQPainterLayer(DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    void releaseBuffers() override;

private:
    std::shared_ptr<DumbSwapchain> m_swapchain;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
};

class DrmVirtualQPainterLayer : public DrmOutputLayer
{
public:
    DrmVirtualQPainterLayer(DrmVirtualOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    QRegion currentDamage() const override;
    void releaseBuffers() override;

private:
    QImage m_image;
    QRegion m_currentDamage;
    DrmVirtualOutput *const m_output;
};
}
