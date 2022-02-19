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

class QPainterLayer
{
public:
    virtual ~QPainterLayer() = default;

    virtual QImage *image() = 0;
};

class DrmQPainterLayer : public DrmPipelineLayer, public QPainterLayer
{
public:
    DrmQPainterLayer(DrmQPainterBackend *backend, DrmPipeline *pipeline);

    std::optional<QRegion> startRendering() override;
    bool endRendering(const QRegion &damagedRegion) override;
    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    QImage *image() override;

private:
    bool doesSwapchainFit() const;

    QSharedPointer<DumbSwapchain> m_swapchain;
    QRegion m_currentDamage;
};

class DrmVirtualQPainterLayer : public DrmOutputLayer, public QPainterLayer
{
public:
    DrmVirtualQPainterLayer(DrmVirtualOutput *output);

    std::optional<QRegion> startRendering() override;
    bool endRendering(const QRegion &damagedRegion) override;

    QRegion currentDamage() const override;
    QImage *image() override;

private:
    QImage m_image;
    QRegion m_currentDamage;
    DrmVirtualOutput *const m_output;
};

class DrmLeaseQPainterLayer : public DrmPipelineLayer
{
public:
    DrmLeaseQPainterLayer(DrmQPainterBackend *backend, DrmPipeline *pipeline);

    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;

private:
    QSharedPointer<DrmDumbBuffer> m_buffer;
};

}
