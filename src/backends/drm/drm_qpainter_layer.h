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

class QPainterLayer
{
public:
    virtual ~QPainterLayer() = default;

    virtual QImage *image() = 0;
};

class DrmQPainterLayer : public DrmPipelineLayer, QPainterLayer
{
public:
    DrmQPainterLayer(DrmPipeline *pipeline);

    std::optional<QRegion> startRendering() override;
    bool endRendering(const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;
    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<GLTexture> texture() const override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    bool hasDirectScanoutBuffer() const override;
    QImage *image() override;

private:
    bool doesSwapchainFit() const;

    QSharedPointer<DumbSwapchain> m_swapchain;
    QRegion m_currentDamage;
};

class DrmVirtualQPainterLayer : public DrmOutputLayer, QPainterLayer
{
public:
    DrmVirtualQPainterLayer(DrmVirtualOutput *output);

    std::optional<QRegion> startRendering() override;
    bool endRendering(const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;

    QSharedPointer<GLTexture> texture() const override;
    QRegion currentDamage() const override;
    QImage *image() override;

private:
    QImage m_image;
    QRegion m_currentDamage;
    DrmVirtualOutput *const m_output;
};

}
