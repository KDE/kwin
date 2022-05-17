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

void DrmOverlayLayer::setPosition(const QPoint &pos)
{
    m_position = pos;
}

QPoint DrmOverlayLayer::position() const
{
    return m_position;
}

void DrmOverlayLayer::setVisible(bool visible)
{
    m_visible = visible;
}

bool DrmOverlayLayer::isVisible() const
{
    return m_visible;
}
}
