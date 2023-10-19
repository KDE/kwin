/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderlayerdelegate.h"

namespace KWin
{

RenderLayer *RenderLayerDelegate::layer() const
{
    return m_layer;
}

void RenderLayerDelegate::setLayer(RenderLayer *layer)
{
    m_layer = layer;
}

void RenderLayerDelegate::frame()
{
}

QRegion RenderLayerDelegate::prePaint()
{
    return QRegion();
}

void RenderLayerDelegate::postPaint()
{
}

SurfaceItem *RenderLayerDelegate::scanoutCandidate() const
{
    return nullptr;
}

void RenderLayerDelegate::presented(std::chrono::nanoseconds nanos, PresentationMode mode)
{
}

} // namespace KWin
