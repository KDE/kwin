/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class Deleted;
class Shadow;
class Window;

/**
 * The ShadowItem class represents a nine-tile patch server-side drop-shadow.
 */
class KWIN_EXPORT ShadowItem : public Item
{
    Q_OBJECT

public:
    explicit ShadowItem(Shadow *shadow, Window *window, Scene *scene, Item *parent = nullptr);
    ~ShadowItem() override;

    Shadow *shadow() const;

protected:
    WindowQuadList buildQuads() const override;

private Q_SLOTS:
    void handleTextureChanged();
    void updateGeometry();
    void handleWindowClosed(Window *original, Deleted *deleted);

private:
    Window *m_window;
    Shadow *m_shadow = nullptr;
};

} // namespace KWin
