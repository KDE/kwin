/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "itemrenderer_vulkan.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "core/syncobjtimeline.h"
#include "effect/effect.h"
#include "scene/decorationitem.h"
#include "scene/imageitem.h"
#include "scene/item.h"
#include "scene/outlinedborderitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/vulkan/atlas.h"
#include "scene/vulkan/ninepatch.h"
#include "scene/vulkan/texture.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

ItemRendererVulkan::ItemRendererVulkan(VulkanDevice *device)
    : m_device(device)
{
}

ItemRendererVulkan::~ItemRendererVulkan()
{
}

std::unique_ptr<Texture> ItemRendererVulkan::createTexture(GraphicsBuffer *buffer)
{
    return BufferTextureVulkan::create(m_device, buffer);
}

std::unique_ptr<Texture> ItemRendererVulkan::createTexture(const QImage &image)
{
    return ImageTextureVulkan::create(m_device, image);
}

std::unique_ptr<NinePatch> ItemRendererVulkan::createNinePatch(const QImage &image)
{
    return NinePatchVulkan::create(m_device, image);
}

std::unique_ptr<NinePatch> ItemRendererVulkan::createNinePatch(const QImage &topLeftPatch,
                                                               const QImage &topPatch,
                                                               const QImage &topRightPatch,
                                                               const QImage &rightPatch,
                                                               const QImage &bottomRightPatch,
                                                               const QImage &bottomPatch,
                                                               const QImage &bottomLeftPatch,
                                                               const QImage &leftPatch)
{
    return NinePatchVulkan::create(m_device, topLeftPatch, topPatch, topRightPatch, rightPatch, bottomRightPatch, bottomPatch, bottomLeftPatch, leftPatch);
}

std::unique_ptr<Atlas> ItemRendererVulkan::createAtlas(const QList<QImage> &sprites)
{
    return AtlasVulkan::create(m_device, sprites);
}

void ItemRendererVulkan::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    m_commandBuffer = m_device->createCommandBuffer();
}

void ItemRendererVulkan::endFrame()
{
    // FIXME we need to return the sync fd to the output layer / VulkanSwapchain somehow...
    // Maybe the command buffer should be stored and submitted in the output layer instead?
    // FIXME 2: submitting an empty command buffer is not allowed, so we need to guard against it!
    // auto fd = m_device->submit(std::move(m_commandBuffer), FileDescriptor{});
    // if (fd) {
    //     for (const auto &releasePoint : m_releasePoints) {
    //         releasePoint->addReleaseFence(*fd);
    //     }
    // }
    m_releasePoints.clear();
}

// TODO always clip with a scissor region instead?
// or, with a compute shader, simply with the region...
static RenderGeometry clipQuads(const Item *item, const ItemRendererVulkan::RenderContext *context)
{
    const WindowQuadList quads = item->quads();

    const qreal scale = context->renderTargetScale;
    const QPointF itemToDeviceTranslation = context->transformStack.top().map(QPointF(0., 0.))
        - context->viewportOrigin
        + context->renderOffset;

    RenderGeometry geometry;
    geometry.reserve(quads.count() * 6);

    // split all quads in bounding rect with the actual rects in the region
    for (const WindowQuad &quad : std::as_const(quads)) {
        if (context->deviceClip != Region::infinite() && !context->hardwareClipping) {
            // Scale to device coordinates, rounding as needed.
            const RectF deviceBounds = snapToPixelGridF(scaledRect(quad.bounds(), scale));

            for (const Rect &deviceClipRect : context->deviceClip.rects()) {
                const RectF relativeDeviceClipRect = RectF(deviceClipRect).translated(-itemToDeviceTranslation);
                const RectF intersected = relativeDeviceClipRect.intersected(deviceBounds);
                if (intersected.isValid()) {
                    if (deviceBounds == intersected) {
                        // case 1: completely contains, include and do not check other rects
                        geometry.appendWindowQuad(quad, scale);
                        break;
                    }
                    // case 2: intersection
                    geometry.appendSubQuad(quad, intersected, scale);
                }
            }
        } else {
            geometry.appendWindowQuad(quad, scale);
        }
    }

    return geometry;
}

void ItemRendererVulkan::createRenderNode(Item *item, RenderContext *context, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter)
{
    bool hole = false;
    if (filter && filter(item)) {
        if (!holeFilter || !holeFilter(item)) {
            return;
        }
        hole = true;
    }
    const QList<Item *> sortedChildItems = item->sortedChildItems();

    const auto logicalPosition = QVector2D(item->position().x(), item->position().y());
    const auto scale = context->renderTargetScale;

    QMatrix4x4 matrix;
    matrix.translate(roundVector(logicalPosition * scale).toVector3D());
    if (context->transformStack.size() == 1) {
        matrix *= context->rootTransform;
    }
    if (!item->transform().isIdentity()) {
        matrix.scale(scale, scale);
        matrix *= item->transform();
        matrix.scale(1 / scale, 1 / scale);
    }
    context->transformStack.push(context->transformStack.top() * matrix);

    context->opacityStack.push(context->opacityStack.top() * item->opacity());

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->explicitVisible()) {
            createRenderNode(childItem, context, filter, holeFilter);
        }
    }

    if (const BorderRadius radius = item->borderRadius(); !radius.isNull()) {
        const RectF nativeRect = snapToPixelGridF(scaledRect(item->rect(), context->renderTargetScale));
        const BorderRadius nativeRadius = radius.scaled(context->renderTargetScale).rounded();
        context->cornerStack.push({
            .box = nativeRect,
            .radius = nativeRadius,
        });
    } else if (!context->cornerStack.isEmpty()) {
        const auto &top = std::as_const(context->cornerStack).top();
        context->cornerStack.push({
            .box = matrix.inverted().mapRect(top.box),
            .radius = top.radius,
        });
    }

    item->preprocess();

    RenderGeometry geometry = clipQuads(item, context);

    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        if (!geometry.isEmpty()) {
            const auto ninePatch = static_cast<NinePatchVulkan *>(shadowItem->ninePatch());
            if (ninePatch && ninePatch->texture()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .texture = ninePatch->texture(),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = true,
                    .colorDescription = item->colorDescription(),
                    .renderingIntent = item->renderingIntent(),
                    .bufferReleasePoint = nullptr,
                    .paintHole = hole,
                });
                // TODO
                // renderNode.geometry.postProcessTextureCoordinates(ninePatch->texture()->matrix(UnnormalizedCoordinates));
            }
        }
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        if (!geometry.isEmpty()) {
            auto atlas = static_cast<const AtlasVulkan *>(decorationItem->atlas());
            if (atlas && atlas->texture()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .texture = atlas->texture(),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = true,
                    .colorDescription = item->colorDescription(),
                    .renderingIntent = item->renderingIntent(),
                    .bufferReleasePoint = nullptr,
                    .paintHole = hole,
                });
                // TODO
                // renderNode.geometry.postProcessTextureCoordinates(atlas->texture()->matrix(UnnormalizedCoordinates));
            }
        }
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        auto texture = static_cast<TextureVulkan *>(surfaceItem->texture());
        if (texture && texture->texture()) {
            if (!geometry.isEmpty()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .texture = texture->texture(),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = surfaceItem->hasAlphaChannel(),
                    .colorDescription = item->colorDescription(),
                    .renderingIntent = item->renderingIntent(),
                    .bufferReleasePoint = surfaceItem->bufferReleasePoint(),
                    .paintHole = hole,
                    .hasFloatingPointColor = texture->isFloatingPoint(),
                });
                // TODO
                // renderNode.geometry.postProcessTextureCoordinates(texture->planes().at(0)->matrix(UnnormalizedCoordinates));

                if (!context->cornerStack.isEmpty()) {
                    const auto &top = context->cornerStack.top();

                    // renderNode.traits |= ShaderTrait::RoundedCorners;
                    renderNode.hasAlpha = true;
                    renderNode.box = QVector4D(top.box.x() + top.box.width() * 0.5,
                                               top.box.y() + top.box.height() * 0.5,
                                               top.box.width() * 0.5,
                                               top.box.height() * 0.5),
                    renderNode.borderRadius = top.radius.toVector();
                }
            }
        }
    } else if (auto imageItem = qobject_cast<ImageItem *>(item)) {
        if (!geometry.isEmpty()) {
            auto texture = static_cast<TextureVulkan *>(imageItem->texture());
            if (texture && texture->texture()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .texture = texture->texture(),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = imageItem->image().hasAlphaChannel(),
                    .colorDescription = item->colorDescription(),
                    .renderingIntent = item->renderingIntent(),
                    .bufferReleasePoint = nullptr,
                    .paintHole = hole,
                });
                // TODO
                // renderNode.geometry.postProcessTextureCoordinates(texture->planes()[0]->matrix(UnnormalizedCoordinates));
            }
        }
    } else if (auto borderItem = qobject_cast<OutlinedBorderItem *>(item)) {
        // TODO
        // if (!geometry.isEmpty()) {
        //     const BorderOutline outline = borderItem->outline();
        //     const int thickness = std::round(outline.thickness() * context->renderTargetScale);
        //     const RectF outerRect = snapToPixelGridF(scaledRect(borderItem->rect(), context->renderTargetScale));
        //     const RectF innerRect = outerRect.adjusted(thickness, thickness, -thickness, -thickness);
        //     context->renderNodes.append(RenderNode{
        //         .geometry = geometry,
        //         .transformMatrix = context->transformStack.top(),
        //         .opacity = context->opacityStack.top(),
        //         .hasAlpha = true,
        //         .colorDescription = borderItem->colorDescription(),
        //         .renderingIntent = borderItem->renderingIntent(),
        //         .box = QVector4D(innerRect.x() + innerRect.width() * 0.5,
        //                          innerRect.y() + innerRect.height() * 0.5,
        //                          innerRect.width() * 0.5,
        //                          innerRect.height() * 0.5),
        //         .borderRadius = outline.radius().scaled(context->renderTargetScale).rounded().toVector(),
        //         .borderThickness = thickness,
        //         .borderColor = outline.color(),
        //         .paintHole = hole,
        //     });
        // }
    }

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() < 0) {
            continue;
        }
        if (childItem->explicitVisible()) {
            createRenderNode(childItem, context, filter, holeFilter);
        }
    }

    context->transformStack.pop();
    context->opacityStack.pop();
    if (!context->cornerStack.isEmpty()) {
        context->cornerStack.pop();
    }
}

void ItemRendererVulkan::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion)
{
    // TODO if we start a render pass in beginFrame, this could simply do
    // m_commandBuffer.clearAttachments();
    // if we use a compute shader instead, we'd just call the shader without items... not ideal though
}

void ItemRendererVulkan::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter)
{
    if (deviceRegion.isEmpty()) {
        return;
    }

    RenderContext renderContext{
        .projectionMatrix = viewport.projectionMatrix(),
        .rootTransform = data.toMatrix(viewport.scale()), // TODO: unify transforms
        .deviceClip = (deviceRegion & renderTarget.transformedRect()),
        .hardwareClipping = (deviceRegion != Region::infinite() && ((mask & Scene::PAINT_WINDOW_TRANSFORMED) || (mask & Scene::PAINT_SCREEN_TRANSFORMED))) || !viewport.renderOffset().isNull(),
        .renderTargetScale = viewport.scale(),
        .viewportOrigin = viewport.scaledRenderRect().topLeft(),
        .renderOffset = viewport.renderOffset(),
    };

    renderContext.transformStack.push(QMatrix4x4());
    renderContext.opacityStack.push(data.opacity());

    createRenderNode(item, &renderContext, filter, holeFilter);

    int totalVertexCount = 0;
    for (const RenderNode &node : std::as_const(renderContext.renderNodes)) {
        totalVertexCount += node.geometry.count();
    }
    if (totalVertexCount == 0) {
        return;
    }

    // TODO Will we even use vertices for this renderer, instead of item shapes directly?
    // for (int i = 0, v = 0; i < renderContext.renderNodes.count(); i++) {
    //     RenderNode &renderNode = renderContext.renderNodes[i];
    //     renderNode.firstVertex = v;
    //     renderNode.vertexCount = renderNode.geometry.count();
    //     renderNode.geometry.copy(map->subspan(v));
    //     v += renderNode.geometry.count();
    // }

    // The scissor region must be in the render target local coordinate system.
    const QSize bufferOffset = renderTarget.transform().map(QSize(viewport.renderOffset().x(), viewport.renderOffset().y()));
    Region scissorRegion = Rect(QPoint(bufferOffset.width(), bufferOffset.height()), renderTarget.size() - 2 * bufferOffset);
    if (renderContext.hardwareClipping) {
        scissorRegion &= viewport.transform().map(deviceRegion & renderTarget.transformedRect(), renderTarget.transformedSize());
    }

    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];

        // TODO

        if (renderNode.bufferReleasePoint) {
            m_releasePoints.insert(renderNode.bufferReleasePoint);
        }
    }
}

}
