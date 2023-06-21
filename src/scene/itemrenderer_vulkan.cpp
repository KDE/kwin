/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "itemrenderer_vulkan.h"
#include "core/pixelgrid.h"
#include "core/renderviewport.h"
#include "decorationitem.h"
#include "imageitem.h"
#include "shadowitem.h"
#include "surfaceitem.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

ItemRendererVulkan::ItemRendererVulkan()
{
}

void ItemRendererVulkan::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    renderTarget.commandBuffer().reset(vk::CommandBufferResetFlags());
    if (renderTarget.commandBuffer().begin(vk::CommandBufferBeginInfo{}) != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "command buffer begin failed";
        return;
    }
    std::vector<vk::RenderingAttachmentInfo> colorAttachments{
        vk::RenderingAttachmentInfo{
            renderTarget.imageView(),
            vk::ImageLayout::eAttachmentOptimal,
            vk::ResolveModeFlagBits::eNone,
            {}, // resolve image view
            {}, // resolve image layout
            vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eStore,
            vk::ClearColorValue(), // would be used with vk::AttachmentLoadOp::eClear
        },
    };

    renderTarget.commandBuffer().beginRendering(vk::RenderingInfo{
        vk::RenderingFlags(),
        vk::Rect2D(
            vk::Offset2D(0, 0),
            vk::Extent2D(renderTarget.size().width(), renderTarget.size().height())),
        1, // layer count
        0, // view mask
        colorAttachments,
        {}, // depth attachment
        {}, // stencil attachment
    });
    renderTarget.commandBuffer().setViewport(0, vk::Viewport{
                                                    0, 0, // x, y
                                                    float(renderTarget.size().width()), float(renderTarget.size().height()), // width, height
                                                    0, 1, // min depth, max depth
                                                });
}

void ItemRendererVulkan::endFrame(const RenderTarget &renderTarget)
{
    renderTarget.commandBuffer().endRendering();
    if (renderTarget.commandBuffer().end() != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "command buffer end failed";
        return;
    }
}

void ItemRendererVulkan::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region)
{
    if (region.isEmpty()) {
        return;
    }
    std::vector<vk::ClearRect> rects;
    rects.reserve(region.rectCount());
    for (const auto &rect : region) {
        rects.push_back(vk::ClearRect{
            vk::Rect2D{
                vk::Offset2D(rect.x(), rect.y()),
                vk::Extent2D(rect.width(), rect.height()),
            },
            0,
            1,
        });
    }
    renderTarget.commandBuffer().clearAttachments(vk::ClearAttachment{
                                                      vk::ImageAspectFlagBits::eColor,
                                                      0,
                                                      vk::ClearColorValue(0.2f, 0.2f, 0.8f, 1.0f),
                                                  },
                                                  rects);
}

void ItemRendererVulkan::createRenderNodes(Item *item, RenderContext &context)
{
    const auto logicalPosition = QVector2D(item->position().x(), item->position().y());
    const auto scale = context.scale;

    QMatrix4x4 matrix;
    matrix.translate(roundVector(logicalPosition * scale).toVector3D());
    if (context.transformStack.size() == 1) {
        matrix *= context.rootTransform;
    }
    if (!item->transform().isIdentity()) {
        matrix.scale(scale, scale);
        matrix *= item->transform();
        matrix.scale(1 / scale, 1 / scale);
    }
    context.transformStack.push(context.transformStack.top() * matrix);

    const QList<Item *> sortedChildItems = item->sortedChildItems();
    for (const auto child : sortedChildItems) {
        if (child->z() >= 0) {
            break;
        }
        if (child->explicitVisible()) {
            createRenderNodes(child, context);
        }
    }

    item->preprocess();

    const WindowQuadList quads = item->quads();
    RenderGeometry geometry;
    geometry.reserve(quads.count() * 6);
    for (const WindowQuad &quad : std::as_const(quads)) {
        geometry.appendWindowQuad(quad, context.scale);
    }
    const QPointF worldTranslation = context.transformStack.top().map(QPointF(0., 0.));

    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        context.renderNodes.push_back(RenderNode{
            .rect = QRectF(worldTranslation, shadowItem->size()),
            .color = QColor(0, 0, 0),
        });
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        context.renderNodes.push_back(RenderNode{
            .rect = QRectF(worldTranslation, decorationItem->size()),
            .color = QColor(255, 0, 255),
        });
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        SurfacePixmap *pixmap = surfaceItem->pixmap();
        if (pixmap) {
            context.renderNodes.push_back(RenderNode{
                .rect = QRectF(worldTranslation, surfaceItem->size()),
                .color = QColor(0, 255, 0),
            });
        }
    } else if (auto imageItem = qobject_cast<ImageItemOpenGL *>(item)) {
        context.renderNodes.push_back(RenderNode{
            .rect = QRectF(worldTranslation, imageItem->size()),
            .color = QColor(255, 0, 0),
        });
    }

    for (Item *child : sortedChildItems) {
        if (child->z() < 0) {
            continue;
        }
        if (child->explicitVisible()) {
            createRenderNodes(child, context);
        }
    }
    context.transformStack.pop();
}

void ItemRendererVulkan::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data)
{
    if (region.isEmpty()) {
        return;
    }
    RenderContext context = {
        .rootTransform = QMatrix4x4{},
        .scale = viewport.scale(),
        .renderNodes = {},
    };
    context.transformStack.push(QMatrix4x4{});
    createRenderNodes(item, context);

    for (const auto &node : context.renderNodes) {
        const auto rect = node.rect & viewport.renderRect();
        if (rect.width() == 0 || rect.height() == 0) {
            continue;
        }
        std::array rects = {
            vk::ClearRect{
                vk::Rect2D{
                    vk::Offset2D(rect.x(), rect.y()),
                    vk::Extent2D(rect.width(), rect.height()),
                },
                0,
                1,
            },
        };
        renderTarget.commandBuffer().clearAttachments(vk::ClearAttachment{
                                                          vk::ImageAspectFlagBits::eColor,
                                                          0,
                                                          vk::ClearColorValue(node.color.red() / 255.0f, node.color.green() / 255.0f, node.color.blue() / 255.0f, 1.0f),
                                                      },
                                                      rects);
    }
}

std::unique_ptr<ImageItem> ItemRendererVulkan::createImageItem(Item *parent)
{
    return std::make_unique<ImageItemVulkan>(parent);
}

}
