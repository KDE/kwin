/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer.h"

#include <QRegion>

namespace KWin
{

ItemRenderer::ItemRenderer()
{
}

ItemRenderer::~ItemRenderer()
{
}

QPainter *ItemRenderer::painter() const
{
    return nullptr;
}

void ItemRenderer::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
}

void ItemRenderer::endFrame(const RenderTarget &renderTarget)
{
}

} // namespace KWin
