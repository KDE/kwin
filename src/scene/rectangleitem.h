/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class KWIN_EXPORT RectangleItem : public Item
{
    Q_OBJECT

public:
    explicit RectangleItem(Item *parent = nullptr);

    QColor color() const;
    void setColor(const QColor &color);

protected:
    WindowQuadList buildQuads() const override;

private:
    QColor m_color = Qt::black;
};

} // namespace KWin
