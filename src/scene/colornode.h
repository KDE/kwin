/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/rendernode.h"

#include <QColor>

namespace KWin
{

/**
 * The ColorNode type represents a rectangle filled with some color.
 */
class KWIN_EXPORT ColorNode : public RenderNode
{
public:
    ColorNode(const QRectF &rect, QColor color);

    QColor color() const;

    void diff(RenderNode *other, RenderNodeDiff *diff) override;

protected:
    QColor m_color;
};

inline QColor ColorNode::color() const
{
    return m_color;
}

} // namespace KWin
