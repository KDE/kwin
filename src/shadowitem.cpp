/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shadowitem.h"
#include "shadow.h"

namespace KWin
{

ShadowItem::ShadowItem(Shadow *shadow, Scene::Window *window, Item *parent)
    : Item(window, parent)
    , m_shadow(shadow)
{
    connect(shadow, &Shadow::regionChanged, this, &ShadowItem::handleRegionChanged);
    connect(shadow, &Shadow::textureChanged, this, &ShadowItem::handleTextureChanged);

    handleRegionChanged();
    handleTextureChanged();
}

ShadowItem::~ShadowItem()
{
}

Shadow *ShadowItem::shadow() const
{
    return m_shadow.data();
}

void ShadowItem::handleRegionChanged()
{
    const QRect rect = shadow()->shadowRegion().boundingRect();

    setPosition(rect.topLeft());
    setSize(rect.size());
}

void ShadowItem::handleTextureChanged()
{
    scheduleRepaint(rect());
}

} // namespace KWin
