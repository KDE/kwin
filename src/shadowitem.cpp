/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shadowitem.h"
#include "deleted.h"
#include "shadow.h"

namespace KWin
{

ShadowItem::ShadowItem(Shadow *shadow, Toplevel *window, Item *parent)
    : Item(parent)
    , m_window(window)
    , m_shadow(shadow)
{
    connect(window, &Toplevel::windowClosed, this, &ShadowItem::handleWindowClosed);

    connect(shadow, &Shadow::offsetChanged, this, &ShadowItem::updateGeometry);
    connect(shadow, &Shadow::rectChanged, this, &ShadowItem::updateGeometry);
    connect(shadow, &Shadow::textureChanged, this, &ShadowItem::handleTextureChanged);

    updateGeometry();
    handleTextureChanged();
}

ShadowItem::~ShadowItem()
{
}

Shadow *ShadowItem::shadow() const
{
    return m_shadow;
}

void ShadowItem::updateGeometry()
{
    const QRect rect = m_shadow->rect() + m_shadow->offset();

    setPosition(rect.topLeft());
    setSize(rect.size());
    discardQuads();
}

void ShadowItem::handleTextureChanged()
{
    scheduleRepaint(rect());
    discardQuads();
}

void ShadowItem::handleWindowClosed(Toplevel *original, Deleted *deleted)
{
    Q_UNUSED(original)
    m_window = deleted;
}

WindowQuadList ShadowItem::buildQuads() const
{
    return m_shadow->geometry();
}

} // namespace KWin
