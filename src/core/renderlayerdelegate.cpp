/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderlayerdelegate.h"
#include "scene/item.h"

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

void RenderLayerDelegate::hideItem(Item *item)
{
    if (!m_hiddenItems.contains(item)) {
        item->scheduleSceneRepaint(item->rect());
        m_hiddenItems.push_back(item);
    }
}

void RenderLayerDelegate::showItem(Item *item)
{
    if (m_hiddenItems.removeOne(item)) {
        item->scheduleRepaint(item->rect());
    }
}

bool RenderLayerDelegate::shouldRenderItem(Item *item) const
{
    return !m_hiddenItems.contains(item);
}

} // namespace KWin
