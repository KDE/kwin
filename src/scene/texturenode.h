/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "opengl/gltexture.h"
#include "scene/rendernode.h"

namespace KWin
{

class KWIN_EXPORT TextureNode : public RenderNode
{
public:
    TextureNode(GLTexture *texture, OutputTransform transform, const QRectF &sourceRect, const QRectF &targetRect); // FIXME: graphics api independent texture type + multi planar formats
    ~TextureNode() override;

    GLTexture *texture() const;
    OutputTransform transform() const;
    QRectF sourceRect() const;
    QRectF targetRect() const;

    void diff(RenderNode *other, RenderNodeDiff *diff) override;

protected:
    GLTexture *m_texture;
    OutputTransform m_transform;
    QRectF m_sourceRect;
    QRectF m_targetRect;
};

inline GLTexture *TextureNode::texture() const
{
    return m_texture;
}

inline OutputTransform TextureNode::transform() const
{
    return m_transform;
}

inline QRectF TextureNode::sourceRect() const
{
    return m_sourceRect;
}

inline QRectF TextureNode::targetRect() const
{
    return m_targetRect;
}

} // namespace KWin
