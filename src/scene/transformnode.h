/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/rendernode.h"

#include <QTransform>

namespace KWin
{

/**
 * The TransformNode type represents a render node that transforms a render node subtree.
 */
class KWIN_EXPORT TransformNode : public RenderNode
{
public:
    TransformNode(RenderNode *child, const QTransform &transform);
    ~TransformNode() override;

    QTransform transform() const;

    void diff(RenderNode *other, RenderNodeDiff *diff) override;

protected:
    RenderNode *m_child;
    QTransform m_transform;
};

inline QTransform TransformNode::transform() const
{
    return m_transform;
}

} // namespace KWin
