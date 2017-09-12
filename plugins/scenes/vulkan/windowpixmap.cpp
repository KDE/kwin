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

#include "windowpixmap.h"
#include "scene.h"
#include "wayland_server.h"
#include "utils.h"
#include "logging.h"

#include <KWayland/Server/subcompositor_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>

#include <wayland-server-core.h>


namespace KWin
{


VulkanWindowPixmap::VulkanWindowPixmap(Scene::Window *window, VulkanScene *scene)
    : WindowPixmap(window),
      m_scene(scene),
      m_imageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
      m_bufferFormat(UINT32_MAX)
{
}


VulkanWindowPixmap::VulkanWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface,
                                       WindowPixmap *parent,
                                       VulkanScene *scene)
    : WindowPixmap(subSurface, parent),
      m_scene(scene),
      m_imageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
      m_bufferFormat(UINT32_MAX)
{
}


VulkanWindowPixmap::~VulkanWindowPixmap()
{
}


void VulkanWindowPixmap::create()
{
    WindowPixmap::create();
}


void VulkanWindowPixmap::aboutToRender()
{
    // Note that WindowPixmap::surface() and WindowPixmap::subSurface() both return the
    // same surface. The difference between them is that WindowPixmap::subSurface()
    // returns null when the associated surface is a root-surface.

    if (KWayland::Server::SurfaceInterface *surf = surface()) {
        // The window pixmap is associated with a wayland surface
        QRegion damage = surf->trackedDamage();
        surf->resetTrackedDamage();

        // Make sure the wayland buffer reference is up-to-date
        if (!m_image || (subSurface().isNull() && !toplevel()->damage().isEmpty()))
            updateBuffer();

        QPointer<KWayland::Server::BufferInterface> buf = buffer();
        if (buf.isNull())
            return; // FIXME: Fail gracefully

        if (buf->shmBuffer()) {
            wl_shm_buffer *shmBuffer = buf->shmBuffer();
            if (!shmBuffer)
                return;

            const uint32_t bufferFormat = wl_shm_buffer_get_format(shmBuffer);
            const QSize size = buf->size();
            VkImageLayout layout;

            if (m_bufferFormat != bufferFormat) {
                m_image = nullptr;
                m_imageView = nullptr;
                m_memory = nullptr;
            }

            if (m_image) {
                layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else {
                static const struct {
                    wl_shm_format wl_format;
                    VkFormat vk_format;
                    uint32_t bpp;
                    bool alpha;
                    bool rev;
                } table[] = {
                    { WL_SHM_FORMAT_ARGB4444,    VK_FORMAT_B4G4R4A4_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = true  },
                    { WL_SHM_FORMAT_XRGB4444,    VK_FORMAT_B4G4R4A4_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = true  },

                    { WL_SHM_FORMAT_ABGR4444,    VK_FORMAT_R4G4B4A4_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = true  },
                    { WL_SHM_FORMAT_XBGR4444,    VK_FORMAT_R4G4B4A4_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = true  },

                    { WL_SHM_FORMAT_RGBA4444,    VK_FORMAT_R4G4B4A4_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_RGBX4444,    VK_FORMAT_R4G4B4A4_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_BGRA4444,    VK_FORMAT_B4G4R4A4_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_BGRX4444,    VK_FORMAT_B4G4R4A4_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_ARGB1555,    VK_FORMAT_A1R5G5B5_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_XRGB1555,    VK_FORMAT_A1R5G5B5_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_RGBA5551,    VK_FORMAT_R5G5B5A1_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_RGBX5551,    VK_FORMAT_R5G5B5A1_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_BGRA5551,    VK_FORMAT_B5G5R5A1_UNORM_PACK16,    .bpp = 16, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_BGRX5551,    VK_FORMAT_B5G5R5A1_UNORM_PACK16,    .bpp = 16, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_RGB565,      VK_FORMAT_R5G6B5_UNORM_PACK16,      .bpp = 16, .alpha = false, .rev = false },
                    { WL_SHM_FORMAT_BGR565,      VK_FORMAT_B5G6R5_UNORM_PACK16,      .bpp = 16, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_ARGB8888,    VK_FORMAT_B8G8R8A8_UNORM,           .bpp = 32, .alpha = true,  .rev = QSysInfo::ByteOrder == QSysInfo::BigEndian },
                    { WL_SHM_FORMAT_XRGB8888,    VK_FORMAT_B8G8R8A8_UNORM,           .bpp = 32, .alpha = false, .rev = QSysInfo::ByteOrder == QSysInfo::BigEndian },

                    { WL_SHM_FORMAT_ABGR8888,    VK_FORMAT_A8B8G8R8_UNORM_PACK32,    .bpp = 32, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_XBGR8888,    VK_FORMAT_A8B8G8R8_UNORM_PACK32,    .bpp = 32, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_RGBA8888,    VK_FORMAT_A8B8G8R8_UNORM_PACK32,    .bpp = 32, .alpha = true,  .rev = true  },
                    { WL_SHM_FORMAT_RGBX8888,    VK_FORMAT_A8B8G8R8_UNORM_PACK32,    .bpp = 32, .alpha = false, .rev = true  },

                    { WL_SHM_FORMAT_BGRA8888,    VK_FORMAT_B8G8R8A8_UNORM,           .bpp = 32, .alpha = true,  .rev = QSysInfo::ByteOrder != QSysInfo::BigEndian },
                    { WL_SHM_FORMAT_BGRX8888,    VK_FORMAT_B8G8R8A8_UNORM,           .bpp = 32, .alpha = false, .rev = QSysInfo::ByteOrder != QSysInfo::BigEndian },

                    { WL_SHM_FORMAT_ARGB2101010, VK_FORMAT_A2R10G10B10_UNORM_PACK32, .bpp = 32, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_XRGB2101010, VK_FORMAT_A2R10G10B10_UNORM_PACK32, .bpp = 32, .alpha = false, .rev = false },

                    { WL_SHM_FORMAT_ABGR2101010, VK_FORMAT_A2B10G10R10_UNORM_PACK32, .bpp = 32, .alpha = true,  .rev = false },
                    { WL_SHM_FORMAT_XBGR2101010, VK_FORMAT_A2B10G10R10_UNORM_PACK32, .bpp = 32, .alpha = false, .rev = false },
                };

                const auto entry = std::find_if(std::begin(table), std::end(table),
                                                [=](const auto &entry) { return entry.wl_format == bufferFormat; });

                if (entry == std::end(table)) {
                    qCCritical(KWIN_VULKAN) << "Unsupported wl_shm_format";
                    return;
                }

                VkFormat format = entry->vk_format;
                uint32_t bpp    = entry->bpp;
                bool alpha      = entry->alpha;
                bool rev        = entry->rev;

                createTexture(format,
                              size.width(), size.height(),
                              {
                                  .r = rev ? VK_COMPONENT_SWIZZLE_A : VK_COMPONENT_SWIZZLE_R,
                                  .g = rev ? VK_COMPONENT_SWIZZLE_B : VK_COMPONENT_SWIZZLE_G,
                                  .b = rev ? VK_COMPONENT_SWIZZLE_G : VK_COMPONENT_SWIZZLE_B,
                                  .a = alpha ?
                                           rev ? VK_COMPONENT_SWIZZLE_R
                                               : VK_COMPONENT_SWIZZLE_A
                                           : VK_COMPONENT_SWIZZLE_ONE
                              });

                layout = VK_IMAGE_LAYOUT_UNDEFINED;
                damage = QRegion(0, 0, size.width(), size.height());

                m_bufferFormat = bufferFormat;
                m_bitsPerPixel = bpp;
            }

            if (!damage.isEmpty()) {
                updateTexture(shmBuffer, damage, layout);
            }
        } else {
            // TODO
        }

        // Reset the toplevel damage if this window pixmap is associated with the root surface
        if (subSurface().isNull())
            toplevel()->resetDamage();
    } else {
    }
}


void VulkanWindowPixmap::updateTexture(wl_shm_buffer *buffer, const QRegion &damage, VkImageLayout imageLayout)
{
    if (damage.isEmpty())
        return;

    const uint32_t bufferCopyOffsetAlignment = std::max(4u, scene()->optimalBufferCopyOffsetAlignment());
    const uint32_t rowPitchAlignment = std::max(4u, scene()->optimalBufferCopyRowPitchAlignment());
    const uint32_t srcPitch = wl_shm_buffer_get_stride(buffer);
    const uint32_t bytesPerPixel = m_bitsPerPixel / 8;

    size_t allocationSize = 0;
    for (const QRect &rect : damage) {
        const uint32_t pitch = align(rect.width() * bytesPerPixel, rowPitchAlignment);

        allocationSize = align(allocationSize, bufferCopyOffsetAlignment);
        allocationSize += pitch * rect.height();
    }

    VulkanUploadManager *uploadManager = scene()->imageUploadManager();
    const VulkanBufferRange uploadBuffer = uploadManager->allocate(allocationSize, bufferCopyOffsetAlignment);

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(damage.rectCount());

    /* Note: If we switch to using a transfer queue, we need to take
     *       VkQueueFamilyProperties::minImageTransferGranularity into account here.
     */

    const uint8_t * const pixels = static_cast<const uint8_t *>(wl_shm_buffer_get_data(buffer));
    if (!pixels) {
        qCCritical(KWIN_VULKAN()) << "wl_shm_buffer_get_data() returned nullptr";
        return;
    }

    wl_shm_buffer_begin_access(buffer);

    size_t dstOffset = 0;
    for (const QRect &rect : damage) {
        const uint32_t dstPitch = align(rect.width() * bytesPerPixel, rowPitchAlignment);
        const uint32_t dstStride = dstPitch / bytesPerPixel;

        dstOffset = align(dstOffset, bufferCopyOffsetAlignment);

        const uint8_t *src = pixels + rect.y() * srcPitch + rect.x() * bytesPerPixel;
        uint8_t *dst = reinterpret_cast<uint8_t *>(uploadBuffer.data()) + dstOffset;
        for (int y = 0; y < rect.height(); y++) {
            memcpy(dst, src, rect.width() * bytesPerPixel);
            src += srcPitch;
            dst += dstPitch;
        }

        regions.emplace_back(VkBufferImageCopy {
                                 .bufferOffset = uploadBuffer.offset() + dstOffset,
                                 .bufferRowLength = dstStride,
                                 .bufferImageHeight = uint32_t(rect.height()),
                                 .imageSubresource = {
                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .mipLevel = 0,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1
                                 },
                                 .imageOffset = {
                                     .x = rect.x(),
                                     .y = rect.y(),
                                     .z = 0
                                 },
                                 .imageExtent = {
                                     .width = uint32_t(rect.width()),
                                     .height = uint32_t(rect.height()),
                                     .depth = 1
                                 },
                             });

        dstOffset += dstPitch * rect.height();
    }

    wl_shm_buffer_end_access(buffer);

    // Record the copy commands in the setup command buffer
    auto cmd = scene()->setupCommandBuffer();

    uint32_t srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    uint32_t srcAccessMask = 0;

    if (imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    // Transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    cmd->pipelineBarrier(srcStageMask,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         {},
                         {},
                         {
                             {
                                 .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                 .pNext               = nullptr,
                                 .srcAccessMask       = srcAccessMask,
                                 .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                                 .oldLayout           = imageLayout,
                                 .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .image               = m_image->handle(),
                                 .subresourceRange    =
                                 {
                                     .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel    = 0,
                                     .levelCount      = 1,
                                     .baseArrayLayer  = 0,
                                     .layerCount      = 1
                                 }
                             }
                         });

    // Copy from the upload buffer to the image
    cmd->copyBufferToImage(uploadBuffer.handle(), m_image->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());

    // Transition the image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    cmd->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         {},
                         {},
                         {
                             {
                                 .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                 .pNext               = nullptr,
                                 .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                                 .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
                                 .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .image               = m_image->handle(),
                                 .subresourceRange    =
                                 {
                                     .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel    = 0,
                                     .levelCount      = 1,
                                     .baseArrayLayer  = 0,
                                     .layerCount      = 1
                                 }
                             }
                         });

    scene()->addBusyReference(m_image);
    scene()->addBusyReference(m_memory);
}


void VulkanWindowPixmap::createTexture(VkFormat format, uint32_t width, uint32_t height, const VkComponentMapping &swizzle)
{
    m_image = std::make_shared<VulkanImage>(scene()->device(),
                                            VkImageCreateInfo {
                                                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = 0,
                                                .imageType = VK_IMAGE_TYPE_2D,
                                                .format = format,
                                                .extent = { width, height, 1 },
                                                .mipLevels = 1,
                                                .arrayLayers = 1,
                                                .samples = VK_SAMPLE_COUNT_1_BIT,
                                                .tiling = VK_IMAGE_TILING_OPTIMAL,
                                                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
                                                        .format = format,
                                                        .components = swizzle,
                                                        .subresourceRange = {
                                                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0,
                                                            .levelCount = 1,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1
                                                        }
                                                    });
}


WindowPixmap *VulkanWindowPixmap::createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface)
{
    return new VulkanWindowPixmap(subSurface, this, m_scene);
}


bool VulkanWindowPixmap::isValid() const
{
    return WindowPixmap::isValid();
}


} // namespace KWin
