/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/transformnode.h"

namespace KWin
{

static quint64 s_generation = 0;

TransformNode::TransformNode(RenderNode *child, const QTransform &transform)
    : RenderNode(RenderNodeType::Transform, ++s_generation)
    , m_child(child)
    , m_transform(transform)
{
    child->ref();

    m_boundingRect = transform.mapRect(child->boundingRect());
}

TransformNode::~TransformNode()
{
    m_child->unref();
}

void TransformNode::diff(RenderNode *other, RenderNodeDiff *diff)
{
    if (other->type() != RenderNodeType::Transform) {
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
        return;
    }

    TransformNode *otherNode = static_cast<TransformNode *>(other);
    if (otherNode->generation() == generation()) {
        return;
    }

    if (otherNode->transform() != transform()) {
        diff->damage += other->boundingRect().toAlignedRect();
        diff->damage += boundingRect().toAlignedRect();
        return;
    }

    RenderNodeDiff diff1;
    m_child->diff(otherNode->m_child, &diff1);
    diff->damage += m_transform.map(diff1.damage);
}

} // namespace KWin
