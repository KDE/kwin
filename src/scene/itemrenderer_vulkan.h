/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "itemgeometry.h"
#include "itemrenderer.h"
#include "platformsupport/scenes/vulkan/vulkan_backend.h"

#include <QMatrix4x4>
#include <stack>

namespace KWin
{

class ItemRendererVulkan : public ItemRenderer
{
public:
    explicit ItemRendererVulkan();

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame(const RenderTarget &renderTarget) override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

    std::unique_ptr<ImageItem> createImageItem(Item *parent = nullptr) override;

private:
    struct RenderNode
    {
        VulkanTexture *texture = nullptr;
        RenderGeometry geometry;
        QMatrix4x4 transformationMatrix;
        QRectF rect;
        QColor color;
    };
    struct RenderContext
    {
        QMatrix4x4 rootTransform;
        double scale;
        std::stack<QMatrix4x4> transformStack;
        std::vector<RenderNode> renderNodes;
    };
    void createRenderNodes(Item *item, RenderContext &context);
};

}
