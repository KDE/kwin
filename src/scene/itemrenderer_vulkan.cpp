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
#include "scene/scene.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/vulkan/atlas.h"
#include "scene/vulkan/ninepatch.h"
#include "scene/vulkan/texture.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_shader.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

ItemRendererVulkan::ItemRendererVulkan(VulkanDevice *device)
    : m_device(device)
    , m_shader(VulkanShader::compileFromPath(device, "/home/xaver/kde/src/kwin/src/scene/vulkan/composite.spv"))
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

    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        const auto ninePatch = static_cast<NinePatchVulkan *>(shadowItem->ninePatch());
        if (ninePatch && ninePatch->texture()) {
            Rect viewRect = RectF(context->transformStack.top().mapRect(item->rect().scaled(scale))).rounded();
            context->uniform.push_back(ItemData{
                .rect = QVector4D(viewRect.left(), viewRect.top(), viewRect.right(), viewRect.bottom()),
                .color = QVector4D(1.0, 1.0f, 1.0f, 1.0f),
            });
        }
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        auto atlas = static_cast<const AtlasVulkan *>(decorationItem->atlas());
        if (atlas && atlas->texture()) {
            Rect viewRect = RectF(context->transformStack.top().mapRect(item->rect().scaled(scale))).rounded();
            context->uniform.push_back(ItemData{
                .rect = QVector4D(viewRect.left(), viewRect.top(), viewRect.right(), viewRect.bottom()),
                .color = QVector4D(0.5f, 0.5f, 0.5f, 1.0f),
            });
        }
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        auto texture = static_cast<TextureVulkan *>(surfaceItem->texture());
        if (texture && texture->texture()) {
            Rect viewRect = RectF(context->transformStack.top().mapRect(item->rect().scaled(scale))).rounded();
            context->uniform.push_back(ItemData{
                .rect = QVector4D(viewRect.left(), viewRect.top(), viewRect.right(), viewRect.bottom()),
                .color = QVector4D(viewRect.left() / float(context->viewportSize.width()),
                                   viewRect.right() / float(context->viewportSize.width()),
                                   viewRect.top() / float(context->viewportSize.height()),
                                   1.0f),
            });
        }
    } else if (auto imageItem = qobject_cast<ImageItem *>(item)) {
        auto texture = static_cast<TextureVulkan *>(imageItem->texture());
        if (texture && texture->texture()) {
            Rect viewRect = RectF(context->transformStack.top().mapRect(item->rect().scaled(scale))).rounded();
            context->uniform.push_back(ItemData{
                .rect = QVector4D(viewRect.left(), viewRect.top(), viewRect.right(), viewRect.bottom()),
                .color = QVector4D(0.2f, 0.8f, 0.2f, 1.0f),
            });
        }
    } else if (auto borderItem = qobject_cast<OutlinedBorderItem *>(item)) {
        Rect viewRect = RectF(context->transformStack.top().mapRect(item->rect().scaled(scale))).rounded();
        context->uniform.push_back(ItemData{
            .rect = QVector4D(viewRect.left(), viewRect.top(), viewRect.right(), viewRect.bottom()),
            .color = QVector4D(1.0f, 0.0f, 0.0f, 1.0f),
        });
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
        .viewportSize = viewport.deviceSize(),
    };

    renderContext.transformStack.push(QMatrix4x4());
    renderContext.opacityStack.push(data.opacity());

    createRenderNode(item, &renderContext, filter, holeFilter);

    // TODO actually use this
    const QSize bufferOffset = renderTarget.transform().map(QSize(viewport.renderOffset().x(), viewport.renderOffset().y()));
    Region scissorRegion = Rect(QPoint(bufferOffset.width(), bufferOffset.height()), renderTarget.size() - 2 * bufferOffset);
    if (renderContext.hardwareClipping) {
        scissorRegion &= viewport.transform().map(deviceRegion & renderTarget.transformedRect(), renderTarget.transformedSize());
    }

    if (renderContext.uniform.empty()) {
        return;
    }

    const vk::DeviceSize bufferSize = renderContext.uniform.size() * sizeof(ItemData);
    vk::BufferCreateInfo bufferInfo{
        vk::BufferCreateFlags(),
        bufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
    };
    auto uboMemory = m_device->allocateMemory(bufferInfo, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (!*uboMemory) {
        return;
    }
    auto [bufResult, buffer] = m_device->logicalDevice().createBuffer(bufferInfo);
    if (bufResult != vk::Result::eSuccess) {
        return;
    }
    buffer.bindMemory(uboMemory, 0);
    auto [mapResult, dataPtr] = uboMemory.mapMemory(0, bufferSize);
    if (mapResult != vk::Result::eSuccess) {
        return;
    }
    std::memcpy(dataPtr, renderContext.uniform.data(), bufferSize);
    uboMemory.unmapMemory();

    vk::DescriptorBufferInfo descriptorBufferInfo{
        *buffer,
        0,
        bufferSize,
    };
    m_device->logicalDevice().updateDescriptorSets(vk::WriteDescriptorSet{
                                                       *m_shader->bufferSet(),
                                                       0,
                                                       0,
                                                       vk::DescriptorType::eStorageBuffer,
                                                       {},
                                                       descriptorBufferInfo,
                                                   },
                                                   {});

    vk::DescriptorImageInfo descriptorImageInfo{
        {}, // sampler
        renderTarget.vulkanTexture()->view(),
        vk::ImageLayout::eGeneral,
    };
    m_device->logicalDevice().updateDescriptorSets(vk::WriteDescriptorSet{
                                                       *m_shader->imageSet(),
                                                       0,
                                                       0,
                                                       vk::DescriptorType::eStorageImage,
                                                       descriptorImageInfo,
                                                   },
                                                   {});

    // FIXME we need to require a compute queue for this...
    auto cmd = m_device->createCommandBuffer();
    cmd.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    cmd.bindShadersEXT(vk::ShaderStageFlagBits::eCompute, *m_shader->handle());
    std::vector<vk::DescriptorSet> descriptorSets{
        *m_shader->imageSet(),
        *m_shader->bufferSet(),
    };
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        *m_shader->pipelineLayout(),
        0,
        descriptorSets,
        {});
    cmd.pushConstants<uint32_t>(*m_shader->pipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, renderContext.uniform.size());
    cmd.dispatch(renderTarget.size().width() / 16, renderTarget.size().height() / 16, 1);
    cmd.end();
    if (!m_device->submitBlocking(std::move(cmd))) {
        qWarning() << "submit failed!";
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
