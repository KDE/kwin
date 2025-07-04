/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "scene/borderoutline.h"
#include "scene/item.h"

namespace KWin
{

class KWIN_EXPORT OutlinedBorderItem : public Item
{
    Q_OBJECT

public:
    explicit OutlinedBorderItem(const QRectF &innerRect, const BorderOutline &outline, Item *parent = nullptr);

    QRectF innerRect() const;
    void setInnerRect(const QRectF &rect);

    BorderOutline outline() const;
    void setOutline(const BorderOutline &outline);

protected:
    WindowQuadList buildQuads() const override;

private:
    QRectF m_innerRect;
    BorderOutline m_outline;
};

} // namespace KWin
