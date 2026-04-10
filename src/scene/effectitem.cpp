/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene/effectitem.h"
#include "opengl/eglswapchain.h"

namespace KWin
{

EffectItem::EffectItem(Item *parentItem)
    : BufferItem(parentItem)
{
    // TODO this might not be needed for some effects?
    addEffect();
}

bool EffectItem::needsBackgroundTexture() const
{
    return m_needsBackgroundTexture;
}

void EffectItem::setNeedsBackgroundTexture(bool background)
{
    m_needsBackgroundTexture = background;
    if (!m_needsBackgroundTexture) {
        m_swapchain.reset();
    }
}

void EffectItem::setBackgroundCache(std::shared_ptr<EglSwapchain> &&swapchain)
{
    m_swapchain = std::move(swapchain);
}

EglSwapchain *EffectItem::backgroundCache() const
{
    return m_swapchain.get();
}

Texture *EffectItem::prepareRendering(GLTexture *background) const
{
    return texture();
}

}

#include "moc_effectitem.cpp"
