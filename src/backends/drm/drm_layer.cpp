/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"

#include "drm_buffer.h"
#include "drm_pipeline.h"

#include <QMatrix4x4>

namespace KWin
{

DrmOutputLayer::~DrmOutputLayer() = default;

QRegion DrmOutputLayer::currentDamage() const
{
    return {};
}

QSharedPointer<GLTexture> DrmOutputLayer::texture() const
{
    return nullptr;
}

QRect DrmPipelineLayer::pixelGeometry() const
{
    return currentBuffer() ? QRect(QPoint(0, 0), currentBuffer()->size()) : QRect();
}

bool DrmPipelineLayer::checkTestBuffer()
{
    return false;
}

bool DrmPipelineLayer::hasDirectScanoutBuffer() const
{
    return false;
}

void DrmPipelineLayer::pageFlipped()
{
}

bool DrmOverlayLayer::needsCompositing() const
{
    return m_needsCompositing;
}

void DrmOverlayLayer::setNeedsCompositing(bool needsCompositing)
{
    m_needsCompositing = needsCompositing;
}
}

#include "drm_layer.moc"
