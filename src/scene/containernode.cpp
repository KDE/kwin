/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/containernode.h"

namespace KWin
{

static quint64 s_generation = 0;

ContainerNode::ContainerNode(const QList<RenderNode *> &nodes)
    : RenderNode(RenderNodeType::Container, ++s_generation)
    , m_nodes(nodes)
{
    for (RenderNode *node : std::as_const(nodes)) {
        node->ref();
        m_boundingRect |= node->boundingRect();
    }
}

ContainerNode::~ContainerNode()
{
    for (RenderNode *node : std::as_const(m_nodes)) {
        node->unref();
    }
}

void ContainerNode::diff(RenderNode *other, RenderNodeDiff *diff)
{
    if (other->type() != RenderNodeType::Container) {
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
        return;
    }

    ContainerNode *otherNode = static_cast<ContainerNode *>(other);
    if (otherNode->generation() == generation()) {
        return;
    }

    if (otherNode->nodes() != nodes()) { // TODO: partial updates
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
        return;
    }
}

} // namespace KWin
