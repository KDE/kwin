/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/colornode.h"

namespace KWin
{

static quint64 s_generation = 0;

ColorNode::ColorNode(const QRectF &rect, QColor color)
    : RenderNode(RenderNodeType::Color, ++s_generation)
{
    m_boundingRect = rect;
}

void ColorNode::diff(RenderNode *other, RenderNodeDiff *diff)
{
    if (other->type() != RenderNodeType::Color) {
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
    } else {
        ColorNode *otherNode = static_cast<ColorNode *>(other);
        if (otherNode->color() != color() || otherNode->boundingRect() != boundingRect()) {
            diff->damage += otherNode->boundingRect().toAlignedRect();
            diff->damage += boundingRect().toAlignedRect();
        }
    }
}

} // namespace KWin
