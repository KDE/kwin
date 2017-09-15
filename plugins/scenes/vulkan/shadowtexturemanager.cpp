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

#include "shadowtexturemanager.h"
#include "scene.h"
#include "utils.h"


size_t std::hash<QImage>::operator()(const QImage &image) const
{
    size_t hash = 0;

    hash = image.format();

    hash = ((hash >> (8 * sizeof(size_t) - 1)) | (hash << 1)) ^ image.width();
    hash = ((hash >> (8 * sizeof(size_t) - 1)) | (hash << 1)) ^ image.height();

    const uint32_t *pixel = reinterpret_cast<const uint32_t *>(image.bits());

    for (int i = 0; i < image.byteCount() / 4; i++)
        hash = ((hash >> (8 * sizeof(size_t) - 1)) | (hash << 1)) ^ pixel[i];

    return hash;
}



// ------------------------------------------------------------------



namespace KWin {



ShadowTextureManager::ShadowTextureManager(VulkanScene *scene)
    : m_scene(scene)
{
}


ShadowTextureManager::~ShadowTextureManager()
{
}


std::shared_ptr<ShadowData> ShadowTextureManager::lookup(const QImage &image)
{
    auto it = m_hashTable.find(image);
    if (it != m_hashTable.end()) {
        std::weak_ptr<ShadowData> &data = it->second.data;
        return data.lock();
    }

    // Upload image
    auto data = createTexture(image);
    data->id = ++m_serial;

    m_hashTable.insert({image, {data->id, data}});

    return data;
}


void ShadowTextureManager::remove(InternalShadowData *data)
{
    auto it = std::find_if(m_hashTable.begin(), m_hashTable.end(),
                           [&](const auto &entry) { return entry.second.id == data->id; });

    if (it != m_hashTable.end())
        m_hashTable.erase(it);

    delete data;
}


std::shared_ptr<ShadowTextureManager::InternalShadowData> ShadowTextureManager::createTexture(const QImage &image)
{
    auto texture = std::shared_ptr<InternalShadowData>(new InternalShadowData, [this](auto ptr) { remove(ptr); });

    const bool alphaOnly = image.depth() == 8;
    const VkFormat format = alphaOnly ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;

    const VkComponentMapping alphaSwizzle {
        .r = VK_COMPONENT_SWIZZLE_ZERO,
        .g = VK_COMPONENT_SWIZZLE_ZERO,
        .b = VK_COMPONENT_SWIZZLE_ZERO,
        .a = VK_COMPONENT_SWIZZLE_R,
    };

    const VkComponentMapping identitySwizzle {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };

    const uint32_t width = image.width();
    const uint32_t height = image.height();

    texture->image = std::make_shared<VulkanImage>(scene()->device(),
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
                                                       .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                       .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                       .queueFamilyIndexCount = 0,
                                                       .pQueueFamilyIndices = nullptr,
                                                       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
                                                   });


    texture->memory = scene()->deviceMemoryAllocator()->allocateMemory(texture->image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

    texture->imageView = std::make_shared<VulkanImageView>(scene()->device(),
                                                           VkImageViewCreateInfo {
                                                               .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .flags = 0,
                                                               .image = texture->image->handle(),
                                                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                               .format = format,
                                                               .components = alphaOnly ? alphaSwizzle : identitySwizzle,
                                                               .subresourceRange = {
                                                                   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                   .baseMipLevel = 0,
                                                                   .levelCount = 1,
                                                                   .baseArrayLayer = 0,
                                                                   .layerCount = 1
                                                               }
                                                           });

    const uint32_t pitch = align(image.bytesPerLine(), scene()->optimalBufferCopyRowPitchAlignment());
    const uint32_t bytesPerPixel = image.depth() / 8;
    auto uploadBuffer = scene()->imageUploadManager()->allocate(pitch * image.height(), scene()->optimalBufferCopyOffsetAlignment());

    uint8_t *dst = static_cast<uint8_t *>(uploadBuffer.data());
    for (int y = 0; y < image.height(); y++) {
        memcpy(dst, image.scanLine(y), image.bytesPerLine());
        dst += pitch;
    }

    const VkBufferImageCopy region {
        .bufferOffset = uploadBuffer.offset(),
        .bufferRowLength = pitch / bytesPerPixel,
        .bufferImageHeight = (uint32_t) image.height(),
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {
            .x = 0,
            .y = 0,
            .z = 0
        },
        .imageExtent = {
            .width = uint32_t(image.width()),
            .height = uint32_t(image.height()),
            .depth = 1
        },
    };

    auto cmd = scene()->setupCommandBuffer();

    // Transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    cmd->pipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         {},
                         {},
                         {
                             {
                                 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                 .pNext = nullptr,
                                 .srcAccessMask = 0,
                                 .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .image = texture->image->handle(),
                                 .subresourceRange = {
                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1
                                 }
                             }
                         });


    cmd->copyBufferToImage(uploadBuffer.handle(), texture->image->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition the image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    cmd->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         {},
                         {},
                         {
                             {
                                 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                 .pNext = nullptr,
                                 .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                 .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                 .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .image = texture->image->handle(),
                                 .subresourceRange = {
                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1
                                 }
                             }
                         });

    scene()->addBusyReference(texture->image);
    scene()->addBusyReference(texture->memory);

    return texture;
}


} // namespace KWin
