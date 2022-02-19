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

void DrmOutputLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
}

bool DrmOutputLayer::scanout(SurfaceItem *surfaceItem)
{
    Q_UNUSED(surfaceItem)
    return false;
}

std::optional<QRegion> DrmOutputLayer::startRendering()
{
    return {};
}

bool DrmOutputLayer::endRendering(const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
    return false;
}

QRegion DrmOutputLayer::currentDamage() const
{
    return {};
}

QSharedPointer<GLTexture> DrmOutputLayer::texture() const
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

}

#include "drm_layer.moc"
