/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer.h"

namespace KWin
{

ItemRenderer::ItemRenderer(RenderDevice *device)
    : m_renderDevice(device)
{
}

ItemRenderer::~ItemRenderer()
{
}

void ItemRenderer::setLayerDebugging(bool enable)
{
}

RenderDevice *ItemRenderer::renderDevice() const
{
    return m_renderDevice;
}

} // namespace KWin
