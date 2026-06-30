/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer.h"

namespace KWin
{

ItemRenderer::ItemRenderer()
{
}

ItemRenderer::~ItemRenderer()
{
}

void ItemRenderer::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
}

void ItemRenderer::endFrame()
{
}

void ItemRenderer::setLayerDebugging(bool enable)
{
}

} // namespace KWin
