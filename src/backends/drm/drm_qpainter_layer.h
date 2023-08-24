/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
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
    DrmQPainterLayer(DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    void releaseBuffers() override;
    quint32 format() const override;
    std::chrono::nanoseconds queryRenderTime() const override;

private:
    bool doesSwapchainFit() const;

    std::shared_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_currentBuffer;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
    DamageJournal m_damageJournal;
    std::chrono::steady_clock::time_point m_renderStart;
    std::chrono::nanoseconds m_renderTime;
};

class DrmCursorQPainterLayer : public DrmPipelineLayer
{
public:
    DrmCursorQPainterLayer(DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    void releaseBuffers() override;
    std::chrono::nanoseconds queryRenderTime() const override;

private:
    std::shared_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_currentBuffer;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
    std::chrono::steady_clock::time_point m_renderStart;
    std::chrono::nanoseconds m_renderTime;
};

class DrmVirtualQPainterLayer : public DrmOutputLayer
{
public:
    DrmVirtualQPainterLayer(DrmVirtualOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    QRegion currentDamage() const override;
    void releaseBuffers() override;
    std::chrono::nanoseconds queryRenderTime() const override;

private:
    QImage m_image;
    QRegion m_currentDamage;
    DrmVirtualOutput *const m_output;
    std::chrono::steady_clock::time_point m_renderStart;
    std::chrono::nanoseconds m_renderTime;
};
}
