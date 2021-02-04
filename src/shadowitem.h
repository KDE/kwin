/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "item.h"

namespace KWin
{

/**
 * The ShadowItem class represents a nine-tile patch server-side drop-shadow.
 */
class KWIN_EXPORT ShadowItem : public Item
{
    Q_OBJECT

public:
    explicit ShadowItem(Shadow *shadow, Scene::Window *window, Item *parent = nullptr);
    ~ShadowItem() override;

    Shadow *shadow() const;

private Q_SLOTS:
    void handleTextureChanged();
    void handleRegionChanged();

private:
    QScopedPointer<Shadow> m_shadow;
};

} // namespace KWin
