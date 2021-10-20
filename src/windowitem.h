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
class DecorationItem;
class Deleted;
class Shadow;
class ShadowItem;
class SurfaceItem;
class Toplevel;

/**
 * The WindowItem class represents a window in the scene.
 *
 * A WindowItem is made of a surface with client contents and optionally a server-side frame
 * and a drop-shadow.
 */
class KWIN_EXPORT WindowItem : public Item
{
    Q_OBJECT

public:
    explicit WindowItem(Toplevel *window, Item *parent = nullptr);

    SurfaceItem *surfaceItem() const;
    DecorationItem *decorationItem() const;
    ShadowItem *shadowItem() const;
    Toplevel *window() const;

private Q_SLOTS:
    void handleWindowClosed(Toplevel *original, Deleted *deleted);
    void updateSurfaceItem();
    void updateDecorationItem();
    void updateShadowItem();
    void updateSurfacePosition();
    void updateSurfaceVisibility();

private:
    Toplevel *m_window;
    QScopedPointer<SurfaceItem> m_surfaceItem;
    QScopedPointer<DecorationItem> m_decorationItem;
    QScopedPointer<ShadowItem> m_shadowItem;
};

} // namespace KWin
