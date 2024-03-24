/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/texturenode.h"

namespace KWin
{

static quint64 s_generation = 0;

TextureNode::TextureNode(GLTexture *texture, OutputTransform transform, const QRectF &sourceRect, const QRectF &targetRect)
    : RenderNode(RenderNodeType::Texture, ++s_generation)
    , m_texture(texture)
    , m_transform(transform)
    , m_sourceRect(sourceRect)
    , m_targetRect(targetRect)
{
    m_boundingRect = targetRect;
}

TextureNode::~TextureNode()
{
}

void TextureNode::diff(RenderNode *other, RenderNodeDiff *diff)
{
    if (other->type() != RenderNodeType::Texture) {
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
        return;
    }

    TextureNode *otherNode = static_cast<TextureNode *>(other);
    if (otherNode->transform() != transform() || otherNode->sourceRect() != sourceRect() || otherNode->targetRect() != targetRect()) {
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
        return;
    }

    // TODO: support partial updates
    diff->damage += other->boundingRect().toAlignedRect();
    diff->damage += boundingRect().toAlignedRect();
}

} // namespace KWin
