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

void RenderLayerDelegate::frame(OutputFrame *frame)
{
}

QRegion RenderLayerDelegate::prePaint()
{
    return QRegion();
}

void RenderLayerDelegate::postPaint()
{
}

QList<SurfaceItem *> RenderLayerDelegate::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

double RenderLayerDelegate::desiredHdrHeadroom() const
{
    return 1;
}

} // namespace KWin
