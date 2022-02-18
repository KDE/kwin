/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"
#include "drm_pipeline.h"
#include "drm_display_device.h"

#include <QMatrix4x4>

namespace KWin
{

DrmOutputLayer::~DrmOutputLayer() = default;

void DrmOutputLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
}

DrmPipelineLayer::DrmPipelineLayer(DrmPipeline *pipeline)
    : m_pipeline(pipeline)
{
}

}

#include "drm_layer.moc"
