/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QRect>
#include <QRegion>

namespace KWin
{

/**
 * The RenderNodeType enum is used to describe the type of a render node.
 */
enum class RenderNodeType {
    Color,
    Container,
    Transform,
    Texture,
};

struct RenderNodeDiff
{
    QRegion damage;
};

/**
 * The RenderNode type is the base class for all nodes in the render tree.
 */
class KWIN_EXPORT RenderNode
{
public:
    virtual ~RenderNode();

    RenderNodeType type() const;
    quint64 generation() const;

    QRectF boundingRect() const;

    void ref();
    void unref();

    virtual void diff(RenderNode *other, RenderNodeDiff *diff) = 0;

protected:
    RenderNode(RenderNodeType type, quint64 generation);

    RenderNodeType m_type;
    quint64 m_generation = 0;
    QRectF m_boundingRect;
    int m_refCount = 1;
};

inline RenderNodeType RenderNode::type() const
{
    return m_type;
}

inline quint64 RenderNode::generation() const
{
    return m_generation;
}

inline QRectF RenderNode::boundingRect() const
{
    return m_boundingRect;
}

} // namespace KWin
