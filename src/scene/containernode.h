/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/rendernode.h"

#include <QList>

namespace KWin
{

/**
 * The ContainerNode type represents a node that points to one or more child nodes.
 */
class KWIN_EXPORT ContainerNode : public RenderNode
{
public:
    explicit ContainerNode(const QList<RenderNode *> &nodes);
    ~ContainerNode() override;

    QList<RenderNode *> nodes() const;

    void diff(RenderNode *other, RenderNodeDiff *diff) override;

protected:
    QList<RenderNode *> m_nodes;
};

inline QList<RenderNode *> ContainerNode::nodes() const
{
    return m_nodes;
}

} // namespace KWin
