/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "item.h"

namespace KDecoration2
{
class Decoration;
}

namespace KWin
{

/**
 * The DecorationItem class represents a server-side decoration.
 */
class KWIN_EXPORT DecorationItem : public Item
{
    Q_OBJECT

public:
    explicit DecorationItem(KDecoration2::Decoration *decoration, Scene::Window *window, Item *parent = nullptr);

private Q_SLOTS:
    void handleFrameGeometryChanged();

private:
    QPointer<KDecoration2::Decoration> m_decoration;
};

} // namespace KWin
