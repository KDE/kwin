/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderlayerdelegate.h"

namespace KWin
{

RenderLayerDelegate::RenderLayerDelegate(QObject *parent)
    : QObject(parent)
{
}

RenderLayer *RenderLayerDelegate::layer() const
{
    return m_layer;
}

void RenderLayerDelegate::setLayer(RenderLayer *layer)
{
    m_layer = layer;
}

void RenderLayerDelegate::prePaint()
{
}

void RenderLayerDelegate::postPaint()
{
}

SurfaceItem *RenderLayerDelegate::scanoutCandidate() const
{
    return nullptr;
}

} // namespace KWin
