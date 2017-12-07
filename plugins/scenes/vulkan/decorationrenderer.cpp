/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "decorationrenderer.h"
#include "decorationthread.h"
#include "decorations/decoratedclient.h"
#include "descriptorset.h"
#include "client.h"
#include "utils.h"

#include <QPainter>

#include <KDecoration2/Decoration>


namespace KWin
{


VulkanDecorationRenderer::VulkanDecorationRenderer(Decoration::DecoratedClientImpl *client, VulkanScene *scene)
    : Renderer(client),
      m_scene(scene)
{
    connect(this, &Renderer::renderScheduled, client->client(),
            static_cast<void(AbstractClient::*)(const QRect&)>(&AbstractClient::addRepaint));
}


VulkanDecorationRenderer::~VulkanDecorationRenderer()
{
}


void VulkanDecorationRenderer::render()
{
    const QRegion scheduled = getScheduled();
    bool resize = areImageSizesDirty();

    if (scheduled.isEmpty() && !resize)
        return;

    if (resize) {
        resizeAtlas();
        resetImageSizesDirty();
    }

    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    const QRect geometry = resize ? QRect(QPoint(0, 0), client()->client()->geometry().size()) : scheduled.boundingRect();
    qreal dpr = client()->client()->screenScale();
    std::array<std::shared_ptr<VulkanImageView>, 4> views;
    std::array<VkImageMemoryBarrier, 4> barriers;
    std::array<GLVertex2D, 4 * 4> vertices;
    GLVertex2D *v = vertices.data();
    uint32_t count = 0;

    std::vector<VulkanDecorationThread::Task> tasks;
    tasks.reserve(4);

    auto imageAllocator = scene()->stagingImageAllocator();

    auto renderPart = [&](const QRect &section, Qt::Orientation orientation, const QPoint &offset) {
        const QRect r = section.intersected(geometry);
        if (!r.isValid())
            return;

        const uint32_t width  = r.width() * dpr;
        const uint32_t height = r.height() * dpr;

        auto image = imageAllocator->createImage(VkImageCreateInfo {
                                                     .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .imageType = VK_IMAGE_TYPE_2D,
                                                     .format = VK_FORMAT_B8G8R8A8_UNORM,
                                                     .extent = { width, height, 1 },
                                                     .mipLevels = 1,
                                                     .arrayLayers = 1,
                                                     .samples = VK_SAMPLE_COUNT_1_BIT,
                                                     .tiling = VK_IMAGE_TILING_LINEAR,
                                                     .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                     .queueFamilyIndexCount = 0,
                                                     .pQueueFamilyIndices = nullptr,
                                                     .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
                                                 });

        const bool rev = QSysInfo::ByteOrder == QSysInfo::BigEndian;

        auto view = std::make_shared<VulkanImageView>(scene()->device(),
                                                      VkImageViewCreateInfo {
                                                          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                          .pNext = nullptr,
                                                          .flags = 0,
                                                          .image = image->handle(),
                                                          .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                          .format = VK_FORMAT_B8G8R8A8_UNORM,
                                                          .components = {
                                                              .r = rev ? VK_COMPONENT_SWIZZLE_A : VK_COMPONENT_SWIZZLE_R,
                                                              .g = rev ? VK_COMPONENT_SWIZZLE_B : VK_COMPONENT_SWIZZLE_G,
                                                              .b = rev ? VK_COMPONENT_SWIZZLE_G : VK_COMPONENT_SWIZZLE_B,
                                                              .a = rev ? VK_COMPONENT_SWIZZLE_R : VK_COMPONENT_SWIZZLE_A,
                                                          },
                                                          .subresourceRange = {
                                                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1
                                                          }
                                                      });

        barriers[count] = VkImageMemoryBarrier {
                              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                              .pNext = nullptr,
                              .srcAccessMask = 0,
                              .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                              .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                              .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                              .image = image->handle(),
                              .subresourceRange = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1
                              }
                          };

        scene()->addBusyReference(image);
        scene()->addBusyReference(view);

        views[count++] = view;

        const VkSubresourceLayout layout = image->getSubresourceLayout({.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0});

        QImage im(image->data() + layout.offset, width, height, layout.rowPitch, QImage::Format_ARGB32_Premultiplied);
        im.setDevicePixelRatio(dpr);

        if (true) {
            tasks.emplace_back(VulkanDecorationThread::Task {
                                   .client = client(),
                                   .image = std::move(im),
                                   .rect = r
                               });
        } else {
            im.fill(0);

            QPainter p;
            p.begin(&im);
            p.setRenderHint(QPainter::Antialiasing);
            p.setWindow(QRect(r.topLeft(), r.size() * dpr));
            p.setClipRect(r);
            client()->decoration()->paint(&p, r);
            p.end();
        }

        const QRectF nominalDstRect = r.translated(-section.topLeft());

        float u0 = 0;
        float u1 = r.width() * dpr;
        float v0 = 0;
        float v1 = r.height() * dpr;

        float x0 = nominalDstRect.left() * dpr;
        float x1 = nominalDstRect.right() * dpr;
        float y0 = nominalDstRect.top() * dpr;
        float y1 = nominalDstRect.bottom() * dpr;

        float dx = offset.x() * dpr;
        float dy = offset.y() * dpr;
        float xscale = 2.0 / m_image->width();
        float yscale = 2.0 / m_image->height();

        if (orientation == Qt::Vertical) {
            std::swap(dx, dy);
            std::swap(xscale, yscale);
        }

        x0 += dx;
        y0 += dy;
        x1 += dx;
        y1 += dy;

        // Convert to clip-space
        x0 = x0 * xscale - 1.0f;
        y0 = y0 * yscale - 1.0f;
        x1 = x1 * xscale - 1.0f;
        y1 = y1 * yscale - 1.0f;

        if (orientation == Qt::Vertical) {
            // Rotate the image 90° counter-clockwise, and flip it vertically
            *v++ = {{y1, x0}, {u0, v1}}; // Top-right
            *v++ = {{y0, x0}, {u0, v0}}; // Top-left
            *v++ = {{y1, x1}, {u1, v1}}; // Bottom-right
            *v++ = {{y0, x1}, {u1, v0}}; // Bottom-left
        } else {
            *v++ = {{x1, y0}, {u1, v0}}; // Top-right
            *v++ = {{x0, y0}, {u0, v0}}; // Top-left
            *v++ = {{x1, y1}, {u1, v1}}; // Bottom-right
            *v++ = {{x0, y1}, {u0, v1}}; // Bottom-left
        }
    };

    renderPart(left,   Qt::Vertical,   QPoint(0, top.height() + bottom.height() + 2));
    renderPart(top,    Qt::Horizontal, QPoint(0, 0));
    renderPart(right,  Qt::Vertical,   QPoint(0, top.height() + bottom.height() + left.width() + 3));
    renderPart(bottom, Qt::Horizontal, QPoint(0, top.height() + 1));

    if (!tasks.empty())
        scene()->decorationThread()->enqueue(std::move(tasks));

    if (count > 0) {
        uint32_t width = m_image->width();
        uint32_t height = m_image->height();

        const VkClearColorValue clearColor = {
            .float32 = { 0.0f, 0.0f, 0.0f, 0.0f }
        };

        const VkClearValue clearValue = {
            .color = clearColor
        };

        const VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = resize ? scene()->offscreenClearRenderPass() :
                                   scene()->offscreenLoadRenderPass(),
            .framebuffer = m_framebuffer->handle(),
            .renderArea = {
                .offset = { 0,     0      },
                .extent = { width, height }
            },
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };

        const VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = float(width),
            .height = float(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        const VkRect2D scissor {
            .offset = { 0,     0      },
            .extent = { width, height }
        };

        auto uploadManager = scene()->uploadManager();
        auto pipelineManager = scene()->pipelineManager();

        // Note: All image descriptors must have a valid image view, so the final
        //       image view is replicated in any unused descriptors.
        auto set = std::make_shared<DecorationImagesDescriptorSet>(scene()->decorationImagesDescriptorPool());
        set->update(views[std::min(0u, count - 1)],
                    views[std::min(1u, count - 1)],
                    views[std::min(2u, count - 1)],
                    views[std::min(3u, count - 1)],
                    VK_IMAGE_LAYOUT_GENERAL);

        const VulkanBufferRange vbo = uploadManager->upload(vertices.data(), count * 4 * sizeof(GLVertex2D));

        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;

        std::tie(pipeline, pipelineLayout) =
                pipelineManager->pipeline(VulkanPipelineManager::DecorationStagingImages,
                                          VulkanPipelineManager::NoTraits,
                                          VulkanPipelineManager::DescriptorSet,
                                          VulkanPipelineManager::TriangleStrip,
                                          VulkanPipelineManager::OffscreenRenderPass);

        auto cmd = scene()->setupCommandBuffer();

        // Transition the staging images to VK_IMAGE_LAYOUT_GENERAL
        cmd->pipelineBarrier(VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, count, barriers.data());

        cmd->beginRenderPass(&renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        cmd->setViewport(0, 1, &viewport);
        cmd->setScissor(0, 1, &scissor);

        cmd->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmd->bindVertexBuffers(0, { vbo.handle() }, { vbo.offset() } );
        cmd->bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, { set->handle() });

        for (uint32_t i = 0; i < count; i++) {
            cmd->pushConstants(pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &i);
            cmd->draw(4, 1, i * 4, 0);
        }

        cmd->endRenderPass();

        scene()->addBusyReference(set);
        scene()->addBusyReference(m_image);
        scene()->addBusyReference(m_imageView);
        scene()->addBusyReference(m_memory);
        scene()->addBusyReference(m_framebuffer);
    }
}


void VulkanDecorationRenderer::resizeAtlas()
{
    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    QSize size;
    size.rwidth() = std::max(std::max(top.width(), bottom.width()),
                             std::max(left.height(), right.height()));
    size.rheight() = top.height() + bottom.height() +
                     left.width() + right.width() + 3;

    size.rwidth() = align(size.width(), 128);
    size *= client()->client()->screenScale();

    if (size.isEmpty())
        return;

    if (m_image && m_image->size() == size)
        return;

    const uint32_t width = size.width();
    const uint32_t height = size.height();

    m_image = std::make_shared<VulkanImage>(scene()->device(),
                                            VkImageCreateInfo {
                                                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = 0,
                                                .imageType = VK_IMAGE_TYPE_2D,
                                                .format = VK_FORMAT_R8G8B8A8_UNORM,
                                                .extent = { width, height, 1 },
                                                .mipLevels = 1,
                                                .arrayLayers = 1,
                                                .samples = VK_SAMPLE_COUNT_1_BIT,
                                                .tiling = VK_IMAGE_TILING_OPTIMAL,
                                                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                .queueFamilyIndexCount = 0,
                                                .pQueueFamilyIndices = nullptr,
                                                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
                                            });

    m_memory = scene()->deviceMemoryAllocator()->allocateMemory(m_image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

    m_imageView = std::make_shared<VulkanImageView>(scene()->device(),
                                                    VkImageViewCreateInfo {
                                                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                        .pNext = nullptr,
                                                        .flags = 0,
                                                        .image = m_image->handle(),
                                                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                                                        .components = {
                                                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        },
                                                        .subresourceRange = {
                                                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0,
                                                            .levelCount = 1,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1
                                                        }
                                                    });

    const VkImageView attachments[] = { m_imageView->handle() };

    m_framebuffer = std::make_shared<VulkanFramebuffer>(scene()->device(),
                                                        VkFramebufferCreateInfo {
                                                            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                            .pNext = nullptr,
                                                            .flags = 0,
                                                            .renderPass = scene()->offscreenClearRenderPass(),
                                                            .attachmentCount = 1,
                                                            .pAttachments = attachments,
                                                            .width = width,
                                                            .height = height,
                                                            .layers = 1
                                                        });
}


void VulkanDecorationRenderer::reparent(Deleted *deleted)
{
    Renderer::reparent(deleted);
}


} // namespace KWin
