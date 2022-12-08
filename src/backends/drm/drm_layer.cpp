/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"
#include "drm_pipeline.h"

#include <QMatrix4x4>

namespace KWin
{

DrmOutputLayer::~DrmOutputLayer() = default;

QPoint DrmOutputLayer::nativePosition() const
{
    return m_nativePosition;
}

void DrmOutputLayer::setNativePosition(const QPoint &point)
{
    m_nativePosition = point;
}

QPoint DrmOutputLayer::nativeOrigin() const
{
    return m_nativeOrigin;
}

void DrmOutputLayer::setNativeOrigin(const QPoint &point)
{
    m_nativeOrigin = point;
}

QRegion DrmOutputLayer::currentDamage() const
{
    return {};
}

std::shared_ptr<GLTexture> DrmOutputLayer::texture() const
{
    return nullptr;
}

DrmPipelineLayer::DrmPipelineLayer(DrmPipeline *pipeline)
    : m_pipeline(pipeline)
{
}

bool DrmPipelineLayer::hasDirectScanoutBuffer() const
{
    return false;
}

DrmOverlayLayer::DrmOverlayLayer(DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
}

}
