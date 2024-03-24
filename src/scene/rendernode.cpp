/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/rendernode.h"

namespace KWin
{

RenderNode::RenderNode(RenderNodeType type, quint64 generation)
    : m_type(type)
    , m_generation(generation)
{
}

RenderNode::~RenderNode()
{
}

void RenderNode::ref()
{
    ++m_refCount;
}

void RenderNode::unref()
{
    --m_refCount;
    if (m_refCount == 0) {
        delete this;
    }
}

} // namespace KWin
